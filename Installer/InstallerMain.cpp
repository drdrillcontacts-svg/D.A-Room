#ifndef WINVER
#define WINVER 0x0A00
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <compressapi.h>
#include <algorithm>
using std::min;
using std::max;
#include <gdiplus.h>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "resource.h"

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "cabinet.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")

namespace fs = std::filesystem;

namespace
{
constexpr int REF_W = 1738;
constexpr int REF_H = 666;

constexpr UINT WM_INSTALL_PROGRESS = WM_APP + 10;
constexpr UINT WM_INSTALL_DONE     = WM_APP + 11;

enum class Page
{
    Welcome = 0,
    Destination = 1,
    Installing = 2,
    Complete = 3
};

enum HitZone
{
    HIT_NONE = 0,
    HIT_MINIMIZE,
    HIT_CLOSE,
    HIT_PRIMARY_BTN,   // Next / Install / Finish
    HIT_SECONDARY_BTN, // Cancel
    HIT_BACK_BTN,      // Back on Destination page
    HIT_BROWSE_BTN,    // Browse... on Destination page
    HIT_PRESETS_CHK    // Factory Presets checkbox
};

struct RectFRef
{
    float x, y, w, h;
    bool contains(float px, float py) const
    {
        return px >= x && px <= (x + w) && py >= y && py <= (y + h);
    }
};

// Exact measured hit zones on the 1738 x 666 reference canvas
constexpr RectFRef kZoneMinimize      { 1592.0f,  12.0f,  54.0f, 50.0f };
constexpr RectFRef kZoneClose         { 1654.0f,  12.0f,  58.0f, 50.0f };
constexpr RectFRef kZonePrimary       {  848.0f, 537.0f, 393.0f, 69.0f };
constexpr RectFRef kZoneCancelWide    { 1287.0f, 537.0f, 344.0f, 67.0f };
constexpr RectFRef kZoneBack          { 1270.0f, 537.0f, 170.0f, 67.0f };
constexpr RectFRef kZoneCancelCompact { 1455.0f, 537.0f, 176.0f, 67.0f };
constexpr RectFRef kZonePathBox       {  848.0f, 386.0f, 616.0f, 58.0f };
constexpr RectFRef kZoneBrowse        { 1476.0f, 386.0f, 155.0f, 58.0f };
constexpr RectFRef kZoneChkVst3       {  848.0f, 464.0f, 330.0f, 40.0f };
constexpr RectFRef kZoneChkPresets    { 1210.0f, 464.0f, 360.0f, 40.0f };

struct AppState
{
    HWND hwnd = nullptr;
    int winW = 1120;
    int winH = 429;

    std::unique_ptr<Gdiplus::Bitmap> welcomeBmp;
    std::unique_ptr<Gdiplus::Bitmap> baseBmp;

    Page page = Page::Welcome;
    HitZone hoverZone = HIT_NONE;
    HitZone pressedZone = HIT_NONE;

    std::wstring vst3InstallDir = L"C:\\Program Files\\Common Files\\VST3";
    std::wstring presetsInstallDir = L"C:\\ProgramData\\D.A Production\\D.A Room\\Presets";
    bool installPresets = true;

    std::atomic<int> progressPercent { 0 };
    std::mutex statusMutex;
    std::wstring statusText = L"Preparing installation...";
    std::wstring errorMessage;
    bool installSucceeded = false;
    std::thread workerThread;
};

AppState gApp;

std::unique_ptr<Gdiplus::Bitmap> LoadPngFromResource(HINSTANCE hInst, int resId)
{
    HRSRC hRes = FindResourceW(hInst, MAKEINTRESOURCEW(resId), RT_RCDATA);
    if (!hRes)
        return nullptr;
    DWORD sz = SizeofResource(hInst, hRes);
    HGLOBAL hGlob = LoadResource(hInst, hRes);
    if (!hGlob || sz == 0)
        return nullptr;
    const void* data = LockResource(hGlob);
    if (!data)
        return nullptr;

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, sz);
    if (!hMem)
        return nullptr;
    void* dst = GlobalLock(hMem);
    std::memcpy(dst, data, sz);
    GlobalUnlock(hMem);

    IStream* stream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(hMem, TRUE, &stream)))
    {
        GlobalFree(hMem);
        return nullptr;
    }

    std::unique_ptr<Gdiplus::Bitmap> bmp(Gdiplus::Bitmap::FromStream(stream, FALSE));
    stream->Release();
    if (!bmp || bmp->GetLastStatus() != Gdiplus::Ok)
        return nullptr;
    return bmp;
}

std::wstring Utf8ToWide(const std::string& s)
{
    if (s.empty())
        return {};
    const int needed = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (needed <= 0)
        return {};
    std::wstring out(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), needed);
    return out;
}

bool ExtractEmbeddedPayload(HINSTANCE hInst,
                            const std::wstring& vst3RootDir,
                            const std::wstring& presetsRootDir,
                            bool includePresets,
                            HWND notifyHwnd,
                            std::wstring& outError)
{
    HRSRC hRes = FindResourceW(hInst, MAKEINTRESOURCEW(IDR_PAYLOAD_BIN), RT_RCDATA);
    if (!hRes)
    {
        outError = L"Embedded plugin payload resource not found.";
        return false;
    }
    const DWORD resSize = SizeofResource(hInst, hRes);
    HGLOBAL hGlob = LoadResource(hInst, hRes);
    const auto* rawRes = static_cast<const uint8_t*>(hGlob ? LockResource(hGlob) : nullptr);
    if (!rawRes || resSize < 16)
    {
        outError = L"Embedded payload resource is empty or invalid.";
        return false;
    }

    if (std::memcmp(rawRes, "DALZ", 4) != 0)
    {
        outError = L"Invalid payload header signature.";
        return false;
    }

    uint64_t uncompressedLen = 0;
    std::memcpy(&uncompressedLen, rawRes + 4, sizeof(uncompressedLen));
    if (uncompressedLen == 0 || uncompressedLen > 512ULL * 1024ULL * 1024ULL)
    {
        outError = L"Invalid uncompressed payload length.";
        return false;
    }

    DECOMPRESSOR_HANDLE decompressor = nullptr;
    if (!CreateDecompressor(COMPRESS_ALGORITHM_LZMS, nullptr, &decompressor))
    {
        outError = L"Failed to initialize Windows LZMS decompressor.";
        return false;
    }

    std::vector<uint8_t> archive(static_cast<size_t>(uncompressedLen));
    SIZE_T actualDecompressed = 0;
    const BOOL decOk = Decompress(
        decompressor,
        rawRes + 12,
        resSize - 12,
        archive.data(),
        archive.size(),
        &actualDecompressed);
    CloseDecompressor(decompressor);

    if (!decOk || actualDecompressed != archive.size())
    {
        outError = L"Payload decompression failed.";
        return false;
    }

    if (archive.size() < 8 || std::memcmp(archive.data(), "DAR1", 4) != 0)
    {
        outError = L"Corrupted archive table.";
        return false;
    }

    uint32_t entryCount = 0;
    std::memcpy(&entryCount, archive.data() + 4, sizeof(entryCount));
    size_t offset = 8;

    // Normalize vst3RootDir: if user picked "...\D.A Room.vst3", step up to its parent because archive paths start with "D.A Room.vst3/"
    fs::path vst3Base(vst3RootDir);
    if (vst3Base.filename() == L"D.A Room.vst3")
        vst3Base = vst3Base.parent_path();

    fs::path presetsBase(presetsRootDir);

    std::error_code ec;
    const fs::path existingBundle = vst3Base / L"D.A Room.vst3";
    if (fs::exists(existingBundle, ec))
        fs::remove_all(existingBundle, ec);

    for (uint32_t idx = 0; idx < entryCount; ++idx)
    {
        if (offset + 3 > archive.size())
        {
            outError = L"Unexpected end of archive.";
            return false;
        }

        const uint8_t targetRoot = archive[offset++];
        uint16_t pathLen = 0;
        std::memcpy(&pathLen, archive.data() + offset, sizeof(pathLen));
        offset += 2;

        if (offset + pathLen + sizeof(uint64_t) > archive.size())
        {
            outError = L"Unexpected end of archive entry header.";
            return false;
        }

        std::string relUtf8(reinterpret_cast<const char*>(archive.data() + offset), pathLen);
        offset += pathLen;

        uint64_t dataLen = 0;
        std::memcpy(&dataLen, archive.data() + offset, sizeof(dataLen));
        offset += sizeof(uint64_t);

        if (offset + dataLen > archive.size())
        {
            outError = L"Unexpected end of archive entry data.";
            return false;
        }

        const uint8_t* fileBytes = archive.data() + offset;
        offset += static_cast<size_t>(dataLen);

        if (targetRoot == 1 && !includePresets)
            continue;

        const std::wstring relWide = Utf8ToWide(relUtf8);
        fs::path destPath = (targetRoot == 0 ? vst3Base : presetsBase) / fs::path(relWide);

        if (notifyHwnd)
        {
            {
                std::lock_guard<std::mutex> lk(gApp.statusMutex);
                gApp.statusText = L"Installing " + destPath.filename().wstring() + L"...";
            }
            const int pct = static_cast<int>(((idx + 1) * 100) / max<uint32_t>(1, entryCount));
            gApp.progressPercent.store(pct);
            PostMessageW(notifyHwnd, WM_INSTALL_PROGRESS, 0, 0);
            if (idx % 6 == 0)
                Sleep(18); // Smooth visual progress pacing in interactive mode
        }

        fs::create_directories(destPath.parent_path(), ec);
        if (ec)
        {
            outError = L"Cannot create folder: " + destPath.parent_path().wstring() + L" (Run as Administrator).";
            return false;
        }

        std::ofstream ofs(destPath, std::ios::binary | std::ios::trunc);
        if (!ofs)
        {
            outError = L"Cannot write file: " + destPath.wstring() +
                       L"\nClose any open DAW using D.A Room and try again.";
            return false;
        }
        if (dataLen > 0)
        {
            ofs.write(reinterpret_cast<const char*>(fileBytes), static_cast<std::streamsize>(dataLen));
            if (!ofs)
            {
                outError = L"Disk write error on: " + destPath.wstring();
                return false;
            }
        }
        ofs.close();
    }

    gApp.progressPercent.store(100);
    return true;
}

void AddRoundedRect(Gdiplus::GraphicsPath& path, float x, float y, float w, float h, float r)
{
    const float d = min(r * 2.0f, min(w, h));
    path.Reset();
    path.AddArc(x, y, d, d, 180.0f, 90.0f);
    path.AddArc(x + w - d, y, d, d, 270.0f, 90.0f);
    path.AddArc(x + w - d, y + h - d, d, d, 0.0f, 90.0f);
    path.AddArc(x, y + h - d, d, d, 90.0f, 90.0f);
    path.CloseFigure();
}

void DrawPrimaryRedButton(Gdiplus::Graphics& g, const RectFRef& rc, const wchar_t* label, bool hovered, bool pressed)
{
    const float yOff = pressed ? 1.5f : 0.0f;
    const RectFRef b { rc.x, rc.y + yOff, rc.w, rc.h };

    // Soft multi-layer crimson neon outer glow matching Page 1
    for (int i = 4; i >= 1; --i)
    {
        const float expand = static_cast<float>(i) * 3.2f;
        Gdiplus::GraphicsPath glowPath;
        AddRoundedRect(glowPath, b.x - expand, b.y - expand, b.w + expand * 2.0f, b.h + expand * 2.0f, 16.0f + expand * 0.6f);
        Gdiplus::SolidBrush glowBrush(Gdiplus::Color(hovered ? (18 * (5 - i)) : (11 * (5 - i)), 238, 26, 36));
        g.FillPath(&glowBrush, &glowPath);
    }

    Gdiplus::GraphicsPath btnPath;
    AddRoundedRect(btnPath, b.x, b.y, b.w, b.h, 16.0f);

    Gdiplus::LinearGradientBrush fillGrad(
        Gdiplus::PointF(b.x, b.y),
        Gdiplus::PointF(b.x, b.y + b.h),
        hovered ? Gdiplus::Color(255, 255, 58, 66) : Gdiplus::Color(255, 245, 36, 44),
        hovered ? Gdiplus::Color(255, 224, 20, 28) : Gdiplus::Color(255, 210, 16, 24));
    g.FillPath(&fillGrad, &btnPath);

    Gdiplus::Pen rimPen(Gdiplus::Color(150, 255, 130, 136), 1.6f);
    g.DrawPath(&rimPen, &btnPath);

    Gdiplus::Font font(L"Segoe UI", 27.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
    Gdiplus::SolidBrush textBrush(Gdiplus::Color(255, 12, 9, 10));
    Gdiplus::StringFormat sf;
    sf.SetAlignment(Gdiplus::StringAlignmentCenter);
    sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    Gdiplus::RectF textRc(b.x, b.y, b.w, b.h);
    g.DrawString(label, -1, &font, textRc, &sf, &textBrush);
}

void DrawSecondaryDarkButton(Gdiplus::Graphics& g, const RectFRef& rc, const wchar_t* label, bool hovered, bool pressed)
{
    const float yOff = pressed ? 1.5f : 0.0f;
    const RectFRef b { rc.x, rc.y + yOff, rc.w, rc.h };

    Gdiplus::GraphicsPath btnPath;
    AddRoundedRect(btnPath, b.x, b.y, b.w, b.h, 16.0f);

    Gdiplus::SolidBrush fillBrush(hovered ? Gdiplus::Color(245, 22, 24, 30) : Gdiplus::Color(240, 8, 9, 11));
    g.FillPath(&fillBrush, &btnPath);

    Gdiplus::Pen borderPen(hovered ? Gdiplus::Color(255, 118, 124, 136) : Gdiplus::Color(255, 62, 66, 74), 2.0f);
    g.DrawPath(&borderPen, &btnPath);

    Gdiplus::Font font(L"Segoe UI", 26.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
    Gdiplus::SolidBrush textBrush(Gdiplus::Color(255, 244, 246, 250));
    Gdiplus::StringFormat sf;
    sf.SetAlignment(Gdiplus::StringAlignmentCenter);
    sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    Gdiplus::RectF textRc(b.x, b.y, b.w, b.h);
    g.DrawString(label, -1, &font, textRc, &sf, &textBrush);
}

void DrawCheckbox(Gdiplus::Graphics& g, const RectFRef& rc, const wchar_t* label, bool checked, bool hovered, bool locked)
{
    const float boxSz = 26.0f;
    const float bx = rc.x;
    const float by = rc.y + (rc.h - boxSz) * 0.5f;

    Gdiplus::GraphicsPath boxPath;
    AddRoundedRect(boxPath, bx, by, boxSz, boxSz, 6.0f);

    if (checked)
    {
        Gdiplus::SolidBrush fill(locked ? Gdiplus::Color(255, 195, 24, 32)
                                        : (hovered ? Gdiplus::Color(255, 250, 46, 54) : Gdiplus::Color(255, 228, 24, 32)));
        g.FillPath(&fill, &boxPath);
        Gdiplus::Pen checkPen(Gdiplus::Color(255, 255, 255, 255), 2.8f);
        checkPen.SetStartCap(Gdiplus::LineCapRound);
        checkPen.SetEndCap(Gdiplus::LineCapRound);
        g.DrawLine(&checkPen, bx + 6.0f, by + 13.5f, bx + 11.2f, by + 19.0f);
        g.DrawLine(&checkPen, bx + 11.2f, by + 19.0f, bx + 20.5f, by + 7.5f);
    }
    else
    {
        Gdiplus::SolidBrush fill(Gdiplus::Color(235, 12, 14, 18));
        g.FillPath(&fill, &boxPath);
        Gdiplus::Pen border(hovered ? Gdiplus::Color(255, 130, 136, 148) : Gdiplus::Color(255, 68, 72, 82), 1.8f);
        g.DrawPath(&border, &boxPath);
    }

    Gdiplus::Font font(L"Segoe UI", 22.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::SolidBrush txt(locked ? Gdiplus::Color(255, 225, 228, 234) : Gdiplus::Color(255, 242, 244, 248));
    Gdiplus::StringFormat sf;
    sf.SetAlignment(Gdiplus::StringAlignmentNear);
    sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    Gdiplus::RectF textRc(bx + boxSz + 12.0f, rc.y, rc.w - boxSz - 12.0f, rc.h);
    g.DrawString(label, -1, &font, textRc, &sf, &txt);
}

void DrawTwoToneHeadline(Gdiplus::Graphics& g, const wchar_t* topLine, const wchar_t* boldWhitePart, const wchar_t* boldRedPart)
{
    Gdiplus::Font topFont(L"Segoe UI", 48.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::SolidBrush whiteBrush(Gdiplus::Color(255, 244, 246, 250));
    g.DrawString(topLine, -1, &topFont, Gdiplus::PointF(844.0f, 206.0f), &whiteBrush);

    Gdiplus::Font mainFont(L"Segoe UI", 58.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
    Gdiplus::StringFormat sf(Gdiplus::StringFormat::GenericTypographic());
    sf.SetFormatFlags(sf.GetFormatFlags() | Gdiplus::StringFormatFlagsMeasureTrailingSpaces);

    const Gdiplus::PointF origin(846.0f, 268.0f);
    g.DrawString(boldWhitePart, -1, &mainFont, origin, &sf, &whiteBrush);

    Gdiplus::RectF measured;
    g.MeasureString(boldWhitePart, -1, &mainFont, origin, &sf, &measured);

    Gdiplus::SolidBrush redBrush(Gdiplus::Color(255, 236, 28, 38));
    g.DrawString(boldRedPart, -1, &mainFont, Gdiplus::PointF(origin.X + measured.Width, origin.Y), &sf, &redBrush);
}

void RenderPageToBitmap(Gdiplus::Bitmap& target, const AppState& state)
{
    const float sx = static_cast<float>(target.GetWidth()) / static_cast<float>(REF_W);
    const float sy = static_cast<float>(target.GetHeight()) / static_cast<float>(REF_H);

    Gdiplus::Graphics g(&target);
    g.Clear(Gdiplus::Color(0, 0, 0, 0));
    g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
    g.ScaleTransform(sx, sy);

    Gdiplus::Bitmap* bg = (state.page == Page::Welcome) ? state.welcomeBmp.get() : state.baseBmp.get();
    if (bg)
        g.DrawImage(bg, Gdiplus::RectF(0.0f, 0.0f, static_cast<float>(REF_W), static_cast<float>(REF_H)),
                    0.0f, 0.0f, static_cast<float>(bg->GetWidth()), static_cast<float>(bg->GetHeight()),
                    Gdiplus::UnitPixel);

    // Subtle hover highlight on top-right Minimize & Close controls
    if (state.hoverZone == HIT_MINIMIZE || state.hoverZone == HIT_CLOSE)
    {
        const RectFRef& hz = (state.hoverZone == HIT_MINIMIZE) ? kZoneMinimize : kZoneClose;
        Gdiplus::GraphicsPath hp;
        AddRoundedRect(hp, hz.x, hz.y, hz.w, hz.h, 8.0f);
        Gdiplus::SolidBrush hb(state.hoverZone == HIT_CLOSE ? Gdiplus::Color(85, 236, 28, 38)
                                                            : Gdiplus::Color(45, 255, 255, 255));
        g.FillPath(&hb, &hp);
    }

    if (state.page == Page::Welcome)
    {
        // Page 1 uses the exact user artwork; add crisp interactive hover glow when hovering Next / Cancel
        if (state.hoverZone == HIT_PRIMARY_BTN)
        {
            Gdiplus::GraphicsPath gp;
            AddRoundedRect(gp, kZonePrimary.x - 3.0f, kZonePrimary.y - 3.0f, kZonePrimary.w + 6.0f, kZonePrimary.h + 6.0f, 18.0f);
            Gdiplus::Pen glowPen(Gdiplus::Color(150, 255, 95, 102), 2.5f);
            g.DrawPath(&glowPen, &gp);
            Gdiplus::SolidBrush shimmer(Gdiplus::Color(28, 255, 255, 255));
            g.FillPath(&shimmer, &gp);
        }
        else if (state.hoverZone == HIT_SECONDARY_BTN)
        {
            Gdiplus::GraphicsPath cp;
            AddRoundedRect(cp, kZoneCancelWide.x, kZoneCancelWide.y, kZoneCancelWide.w, kZoneCancelWide.h, 16.0f);
            Gdiplus::Pen borderPen(Gdiplus::Color(180, 145, 152, 165), 2.2f);
            g.DrawPath(&borderPen, &cp);
            Gdiplus::SolidBrush shimmer(Gdiplus::Color(22, 255, 255, 255));
            g.FillPath(&shimmer, &cp);
        }
        return;
    }

    if (state.page == Page::Destination)
    {
        DrawTwoToneHeadline(g, L"Select Destination", L"D.A Room ", L"VST3 (64-bit)");

        Gdiplus::Font subFont(L"Segoe UI", 24.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush subBrush(Gdiplus::Color(255, 182, 186, 194));
        g.DrawString(L"Destination folder for your 64-bit Windows VST3 plugin:", -1,
                     &subFont, Gdiplus::PointF(848.0f, 344.0f), &subBrush);

        // Path input display box
        Gdiplus::GraphicsPath pathBox;
        AddRoundedRect(pathBox, kZonePathBox.x, kZonePathBox.y, kZonePathBox.w, kZonePathBox.h, 12.0f);
        Gdiplus::SolidBrush boxFill(Gdiplus::Color(245, 10, 11, 15));
        g.FillPath(&boxFill, &pathBox);
        Gdiplus::Pen boxBorder(Gdiplus::Color(255, 62, 66, 76), 1.8f);
        g.DrawPath(&boxBorder, &pathBox);

        // Crimson left accent indicator inside path box
        Gdiplus::SolidBrush redBar(Gdiplus::Color(255, 236, 28, 38));
        g.FillRectangle(&redBar, kZonePathBox.x + 12.0f, kZonePathBox.y + 13.0f, 4.0f, kZonePathBox.h - 26.0f);

        Gdiplus::Font pathFont(L"Segoe UI", 23.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush pathTextBrush(Gdiplus::Color(255, 242, 245, 250));
        Gdiplus::StringFormat pathSf;
        pathSf.SetAlignment(Gdiplus::StringAlignmentNear);
        pathSf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        pathSf.SetTrimming(Gdiplus::StringTrimmingEllipsisPath);
        pathSf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
        Gdiplus::RectF pathTextRc(kZonePathBox.x + 28.0f, kZonePathBox.y, kZonePathBox.w - 40.0f, kZonePathBox.h);
        g.DrawString(state.vst3InstallDir.c_str(), -1, &pathFont, pathTextRc, &pathSf, &pathTextBrush);

        DrawSecondaryDarkButton(g, kZoneBrowse, L"Browse...",
                                state.hoverZone == HIT_BROWSE_BTN, state.pressedZone == HIT_BROWSE_BTN);

        DrawCheckbox(g, kZoneChkVst3, L"D.A Room VST3 (64-bit)", true, false, true);
        DrawCheckbox(g, kZoneChkPresets, L"Factory Presets (50 Presets)", state.installPresets,
                     state.hoverZone == HIT_PRESETS_CHK, false);

        DrawPrimaryRedButton(g, kZonePrimary, L"Install  \x2192",
                             state.hoverZone == HIT_PRIMARY_BTN, state.pressedZone == HIT_PRIMARY_BTN);
        DrawSecondaryDarkButton(g, kZoneBack, L"Back",
                                state.hoverZone == HIT_BACK_BTN, state.pressedZone == HIT_BACK_BTN);
        DrawSecondaryDarkButton(g, kZoneCancelCompact, L"Cancel",
                                state.hoverZone == HIT_SECONDARY_BTN, state.pressedZone == HIT_SECONDARY_BTN);
        return;
    }

    if (state.page == Page::Installing)
    {
        DrawTwoToneHeadline(g, L"Installing Audio Plugin", L"D.A Room ", L"Setup");

        std::wstring statusCopy;
        {
            std::lock_guard<std::mutex> lk(gApp.statusMutex);
            statusCopy = state.statusText;
        }
        const int pct = max(0, min(100, state.progressPercent.load()));

        Gdiplus::Font subFont(L"Segoe UI", 24.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush subBrush(Gdiplus::Color(255, 192, 196, 204));
        g.DrawString(statusCopy.c_str(), -1, &subFont, Gdiplus::PointF(848.0f, 368.0f), &subBrush);

        // Percentage label on right
        wchar_t pctBuf[16];
        wsprintfW(pctBuf, L"%d%%", pct);
        Gdiplus::Font pctFont(L"Segoe UI", 25.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush redBrush(Gdiplus::Color(255, 242, 34, 42));
        Gdiplus::StringFormat rightSf;
        rightSf.SetAlignment(Gdiplus::StringAlignmentFar);
        g.DrawString(pctBuf, -1, &pctFont, Gdiplus::RectF(1450.0f, 366.0f, 181.0f, 34.0f), &rightSf, &redBrush);

        // Progress track
        const RectFRef track { 848.0f, 418.0f, 783.0f, 28.0f };
        Gdiplus::GraphicsPath trackPath;
        AddRoundedRect(trackPath, track.x, track.y, track.w, track.h, 14.0f);
        Gdiplus::SolidBrush trackFill(Gdiplus::Color(245, 12, 13, 17));
        g.FillPath(&trackFill, &trackPath);
        Gdiplus::Pen trackBorder(Gdiplus::Color(255, 58, 62, 70), 1.6f);
        g.DrawPath(&trackBorder, &trackPath);

        if (pct > 0)
        {
            const float fillW = max(28.0f, track.w * (static_cast<float>(pct) / 100.0f));
            Gdiplus::GraphicsPath fillPath;
            AddRoundedRect(fillPath, track.x, track.y, fillW, track.h, 14.0f);
            Gdiplus::LinearGradientBrush barGrad(
                Gdiplus::PointF(track.x, track.y),
                Gdiplus::PointF(track.x + fillW, track.y),
                Gdiplus::Color(255, 210, 18, 26),
                Gdiplus::Color(255, 255, 48, 56));
            g.FillPath(&barGrad, &fillPath);

            Gdiplus::GraphicsPath glowPath;
            AddRoundedRect(glowPath, track.x - 3.0f, track.y - 3.0f, fillW + 6.0f, track.h + 6.0f, 17.0f);
            Gdiplus::Pen glowPen(Gdiplus::Color(90, 245, 36, 44), 3.0f);
            g.DrawPath(&glowPen, &glowPath);
        }

        Gdiplus::Font hintFont(L"Segoe UI", 22.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush dimBrush(Gdiplus::Color(255, 135, 140, 150));
        g.DrawString(L"Please wait while D.A Production configures D.A Room on your system...",
                     -1, &hintFont, Gdiplus::PointF(848.0f, 470.0f), &dimBrush);
        return;
    }

    if (state.page == Page::Complete)
    {
        if (state.installSucceeded)
        {
            DrawTwoToneHeadline(g, L"Installation Complete", L"D.A Room is ", L"Ready");

            Gdiplus::Font bodyFont(L"Segoe UI", 25.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
            Gdiplus::SolidBrush bodyBrush(Gdiplus::Color(255, 200, 204, 212));
            g.DrawString(L"D.A Room (64-bit VST3) has been installed to:",
                         -1, &bodyFont, Gdiplus::PointF(848.0f, 364.0f), &bodyBrush);

            fs::path finalBundle = fs::path(state.vst3InstallDir);
            if (finalBundle.filename() != L"D.A Room.vst3")
                finalBundle /= L"D.A Room.vst3";

            Gdiplus::Font pathFont(L"Segoe UI", 24.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
            Gdiplus::SolidBrush whiteBrush(Gdiplus::Color(255, 248, 250, 252));
            g.DrawString(finalBundle.wstring().c_str(), -1, &pathFont, Gdiplus::PointF(848.0f, 404.0f), &whiteBrush);

            Gdiplus::Font tipFont(L"Segoe UI", 22.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
            Gdiplus::SolidBrush dimBrush(Gdiplus::Color(255, 156, 162, 172));
            g.DrawString(L"Open your DAW and rescan VST3 plugins to start mixing.",
                         -1, &tipFont, Gdiplus::PointF(848.0f, 456.0f), &dimBrush);

            DrawPrimaryRedButton(g, kZonePrimary, L"Finish",
                                 state.hoverZone == HIT_PRIMARY_BTN, state.pressedZone == HIT_PRIMARY_BTN);
        }
        else
        {
            DrawTwoToneHeadline(g, L"Installation Error", L"Setup ", L"Failed");

            Gdiplus::Font errFont(L"Segoe UI", 23.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
            Gdiplus::SolidBrush errBrush(Gdiplus::Color(255, 245, 150, 155));
            Gdiplus::RectF errRc(848.0f, 364.0f, 780.0f, 140.0f);
            g.DrawString(state.errorMessage.c_str(), -1, &errFont, errRc, nullptr, &errBrush);

            DrawPrimaryRedButton(g, kZonePrimary, L"Retry",
                                 state.hoverZone == HIT_PRIMARY_BTN, state.pressedZone == HIT_PRIMARY_BTN);
            DrawSecondaryDarkButton(g, kZoneCancelWide, L"Close",
                                    state.hoverZone == HIT_SECONDARY_BTN, state.pressedZone == HIT_SECONDARY_BTN);
        }
    }
}

void UpdateLayeredFrame(HWND hwnd)
{
    if (!hwnd || gApp.winW <= 0 || gApp.winH <= 0)
        return;

    Gdiplus::Bitmap canvas(gApp.winW, gApp.winH, PixelFormat32bppPARGB);
    RenderPageToBitmap(canvas, gApp);

    HDC screenDC = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(screenDC);

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = gApp.winW;
    bmi.bmiHeader.biHeight = -gApp.winH; // top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* dibBits = nullptr;
    HBITMAP hDib = CreateDIBSection(memDC, &bmi, DIB_RGB_COLORS, &dibBits, nullptr, 0);
    if (hDib && dibBits)
    {
        HGDIOBJ oldBmp = SelectObject(memDC, hDib);

        Gdiplus::Rect lockRc(0, 0, gApp.winW, gApp.winH);
        Gdiplus::BitmapData bd{};
        if (canvas.LockBits(&lockRc, Gdiplus::ImageLockModeRead, PixelFormat32bppPARGB, &bd) == Gdiplus::Ok)
        {
            const int dstStride = gApp.winW * 4;
            for (int row = 0; row < gApp.winH; ++row)
            {
                std::memcpy(static_cast<uint8_t*>(dibBits) + row * dstStride,
                            static_cast<const uint8_t*>(bd.Scan0) + row * bd.Stride,
                            static_cast<size_t>(dstStride));
            }
            canvas.UnlockBits(&bd);
        }

        RECT wr{};
        GetWindowRect(hwnd, &wr);
        POINT ptDst { wr.left, wr.top };
        SIZE szWnd { gApp.winW, gApp.winH };
        POINT ptSrc { 0, 0 };
        BLENDFUNCTION blend{};
        blend.BlendOp = AC_SRC_OVER;
        blend.BlendFlags = 0;
        blend.SourceConstantAlpha = 255;
        blend.AlphaFormat = AC_SRC_ALPHA;

        UpdateLayeredWindow(hwnd, screenDC, &ptDst, &szWnd, memDC, &ptSrc, 0, &blend, ULW_ALPHA);

        SelectObject(memDC, oldBmp);
        DeleteObject(hDib);
    }

    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);
}

HitZone HitTestRef(float rx, float ry, Page page)
{
    if (kZoneMinimize.contains(rx, ry))
        return HIT_MINIMIZE;
    if (kZoneClose.contains(rx, ry) && page != Page::Installing)
        return HIT_CLOSE;

    if (page == Page::Welcome)
    {
        if (kZonePrimary.contains(rx, ry))
            return HIT_PRIMARY_BTN;
        if (kZoneCancelWide.contains(rx, ry))
            return HIT_SECONDARY_BTN;
    }
    else if (page == Page::Destination)
    {
        if (kZonePrimary.contains(rx, ry))
            return HIT_PRIMARY_BTN;
        if (kZoneBack.contains(rx, ry))
            return HIT_BACK_BTN;
        if (kZoneCancelCompact.contains(rx, ry))
            return HIT_SECONDARY_BTN;
        if (kZoneBrowse.contains(rx, ry))
            return HIT_BROWSE_BTN;
        if (kZoneChkPresets.contains(rx, ry))
            return HIT_PRESETS_CHK;
    }
    else if (page == Page::Complete)
    {
        if (kZonePrimary.contains(rx, ry))
            return HIT_PRIMARY_BTN;
        if (!gApp.installSucceeded && kZoneCancelWide.contains(rx, ry))
            return HIT_SECONDARY_BTN;
    }
    return HIT_NONE;
}

void BrowseForDestinationFolder(HWND owner)
{
    IFileOpenDialog* dlg = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg))) && dlg)
    {
        DWORD opts = 0;
        if (SUCCEEDED(dlg->GetOptions(&opts)))
            dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
        dlg->SetTitle(L"Select VST3 Installation Folder");

        IShellItem* initFolder = nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(gApp.vst3InstallDir.c_str(), nullptr, IID_PPV_ARGS(&initFolder))) && initFolder)
        {
            dlg->SetFolder(initFolder);
            initFolder->Release();
        }

        if (SUCCEEDED(dlg->Show(owner)))
        {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dlg->GetResult(&item)) && item)
            {
                PWSTR pathStr = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &pathStr)) && pathStr)
                {
                    gApp.vst3InstallDir = pathStr;
                    CoTaskMemFree(pathStr);
                }
                item->Release();
            }
        }
        dlg->Release();
    }
}

void StartAsyncInstallation(HWND hwnd)
{
    if (gApp.workerThread.joinable())
        gApp.workerThread.join();

    gApp.page = Page::Installing;
    gApp.progressPercent.store(2);
    {
        std::lock_guard<std::mutex> lk(gApp.statusMutex);
        gApp.statusText = L"Preparing D.A Room VST3 files...";
    }
    UpdateLayeredFrame(hwnd);

    const std::wstring vst3Dir = gApp.vst3InstallDir;
    const std::wstring presetsDir = gApp.presetsInstallDir;
    const bool withPresets = gApp.installPresets;
    HINSTANCE hInst = GetModuleHandleW(nullptr);

    gApp.workerThread = std::thread([hwnd, hInst, vst3Dir, presetsDir, withPresets]()
    {
        std::wstring err;
        const bool ok = ExtractEmbeddedPayload(hInst, vst3Dir, presetsDir, withPresets, hwnd, err);
        {
            std::lock_guard<std::mutex> lk(gApp.statusMutex);
            gApp.installSucceeded = ok;
            gApp.errorMessage = err;
        }
        PostMessageW(hwnd, WM_INSTALL_DONE, ok ? 1 : 0, 0);
    });
}

LRESULT CALLBACK InstallerWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_SETCURSOR:
        {
            SetCursor(LoadCursorW(nullptr, gApp.hoverZone != HIT_NONE ? IDC_HAND : IDC_ARROW));
            return TRUE;
        }

        case WM_MOUSEMOVE:
        {
            TRACKMOUSEEVENT tme{};
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);

            const float rx = static_cast<float>(GET_X_LPARAM(lParam)) * static_cast<float>(REF_W) / static_cast<float>(gApp.winW);
            const float ry = static_cast<float>(GET_Y_LPARAM(lParam)) * static_cast<float>(REF_H) / static_cast<float>(gApp.winH);
            const HitZone hz = HitTestRef(rx, ry, gApp.page);
            if (hz != gApp.hoverZone)
            {
                gApp.hoverZone = hz;
                UpdateLayeredFrame(hwnd);
            }
            return 0;
        }

        case WM_MOUSELEAVE:
        {
            if (gApp.hoverZone != HIT_NONE)
            {
                gApp.hoverZone = HIT_NONE;
                UpdateLayeredFrame(hwnd);
            }
            return 0;
        }

        case WM_LBUTTONDOWN:
        {
            const float rx = static_cast<float>(GET_X_LPARAM(lParam)) * static_cast<float>(REF_W) / static_cast<float>(gApp.winW);
            const float ry = static_cast<float>(GET_Y_LPARAM(lParam)) * static_cast<float>(REF_H) / static_cast<float>(gApp.winH);
            const HitZone hz = HitTestRef(rx, ry, gApp.page);
            if (hz != HIT_NONE)
            {
                gApp.pressedZone = hz;
                SetCapture(hwnd);
                UpdateLayeredFrame(hwnd);
            }
            else
            {
                // Drag frameless window smoothly from anywhere on background
                ReleaseCapture();
                SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            }
            return 0;
        }

        case WM_LBUTTONUP:
        {
            const HitZone wasPressed = gApp.pressedZone;
            if (GetCapture() == hwnd)
                ReleaseCapture();
            gApp.pressedZone = HIT_NONE;

            const float rx = static_cast<float>(GET_X_LPARAM(lParam)) * static_cast<float>(REF_W) / static_cast<float>(gApp.winW);
            const float ry = static_cast<float>(GET_Y_LPARAM(lParam)) * static_cast<float>(REF_H) / static_cast<float>(gApp.winH);
            const HitZone hz = HitTestRef(rx, ry, gApp.page);
            gApp.hoverZone = hz;

            if (wasPressed != HIT_NONE && wasPressed == hz)
            {
                if (hz == HIT_MINIMIZE)
                {
                    ShowWindow(hwnd, SW_MINIMIZE);
                }
                else if (hz == HIT_CLOSE)
                {
                    DestroyWindow(hwnd);
                    return 0;
                }
                else if (gApp.page == Page::Welcome)
                {
                    if (hz == HIT_PRIMARY_BTN)
                        gApp.page = Page::Destination;
                    else if (hz == HIT_SECONDARY_BTN)
                    {
                        DestroyWindow(hwnd);
                        return 0;
                    }
                }
                else if (gApp.page == Page::Destination)
                {
                    if (hz == HIT_PRIMARY_BTN)
                    {
                        StartAsyncInstallation(hwnd);
                        return 0;
                    }
                    else if (hz == HIT_BACK_BTN)
                        gApp.page = Page::Welcome;
                    else if (hz == HIT_SECONDARY_BTN)
                    {
                        DestroyWindow(hwnd);
                        return 0;
                    }
                    else if (hz == HIT_BROWSE_BTN)
                    {
                        BrowseForDestinationFolder(hwnd);
                    }
                    else if (hz == HIT_PRESETS_CHK)
                    {
                        gApp.installPresets = !gApp.installPresets;
                    }
                }
                else if (gApp.page == Page::Complete)
                {
                    if (hz == HIT_PRIMARY_BTN)
                    {
                        if (gApp.installSucceeded)
                        {
                            DestroyWindow(hwnd);
                            return 0;
                        }
                        else
                        {
                            gApp.page = Page::Destination;
                        }
                    }
                    else if (hz == HIT_SECONDARY_BTN)
                    {
                        DestroyWindow(hwnd);
                        return 0;
                    }
                }
            }
            UpdateLayeredFrame(hwnd);
            return 0;
        }

        case WM_KEYDOWN:
        {
            if (wParam == VK_ESCAPE && gApp.page != Page::Installing)
            {
                DestroyWindow(hwnd);
                return 0;
            }
            if (wParam == VK_RETURN)
            {
                if (gApp.page == Page::Welcome)
                {
                    gApp.page = Page::Destination;
                    UpdateLayeredFrame(hwnd);
                }
                else if (gApp.page == Page::Destination)
                {
                    StartAsyncInstallation(hwnd);
                }
                else if (gApp.page == Page::Complete && gApp.installSucceeded)
                {
                    DestroyWindow(hwnd);
                    return 0;
                }
            }
            return 0;
        }

        case WM_INSTALL_PROGRESS:
        {
            UpdateLayeredFrame(hwnd);
            return 0;
        }

        case WM_INSTALL_DONE:
        {
            gApp.page = Page::Complete;
            gApp.hoverZone = HIT_NONE;
            UpdateLayeredFrame(hwnd);
            return 0;
        }

        case WM_DESTROY:
        {
            if (gApp.workerThread.joinable())
                gApp.workerThread.join();
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool GetEncoderClsid(const wchar_t* mimeType, CLSID* pClsid)
{
    UINT num = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0)
        return false;
    std::vector<uint8_t> buf(size);
    auto* info = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buf.data());
    Gdiplus::GetImageEncoders(num, size, info);
    for (UINT i = 0; i < num; ++i)
    {
        if (wcscmp(info[i].MimeType, mimeType) == 0)
        {
            *pClsid = info[i].Clsid;
            return true;
        }
    }
    return false;
}

int RunScreenshotDump(const std::wstring& outDir)
{
    CLSID pngClsid{};
    if (!GetEncoderClsid(L"image/png", &pngClsid))
        return 2;

    std::error_code ec;
    fs::create_directories(outDir, ec);

    Gdiplus::Bitmap bmp(REF_W, REF_H, PixelFormat32bppPARGB);

    gApp.page = Page::Welcome;
    RenderPageToBitmap(bmp, gApp);
    bmp.Save((fs::path(outDir) / L"page1_welcome.png").c_str(), &pngClsid, nullptr);

    gApp.page = Page::Destination;
    RenderPageToBitmap(bmp, gApp);
    bmp.Save((fs::path(outDir) / L"page2_destination.png").c_str(), &pngClsid, nullptr);

    gApp.page = Page::Installing;
    gApp.progressPercent.store(68);
    gApp.statusText = L"Installing D.A Room.vst3...";
    RenderPageToBitmap(bmp, gApp);
    bmp.Save((fs::path(outDir) / L"page3_installing.png").c_str(), &pngClsid, nullptr);

    gApp.page = Page::Complete;
    gApp.installSucceeded = true;
    RenderPageToBitmap(bmp, gApp);
    bmp.Save((fs::path(outDir) / L"page4_complete.png").c_str(), &pngClsid, nullptr);

    return 0;
}
} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    Gdiplus::GdiplusStartupInput gdiplusInput;
    ULONG_PTR gdiplusToken = 0;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusInput, nullptr);

    gApp.welcomeBmp = LoadPngFromResource(hInstance, IDR_WELCOME_PNG);
    gApp.baseBmp = LoadPngFromResource(hInstance, IDR_BASE_PNG);

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool silentMode = false;
    std::wstring screenshotDir;

    if (argv)
    {
        for (int i = 1; i < argc; ++i)
        {
            const std::wstring arg(argv[i]);
            if (arg == L"/S" || arg == L"/s" || arg == L"/VERYSILENT" || arg == L"--silent")
            {
                silentMode = true;
            }
            else if (arg.rfind(L"/DIR=", 0) == 0)
            {
                gApp.vst3InstallDir = arg.substr(5);
            }
            else if (arg == L"/SCREENSHOT" && (i + 1) < argc)
            {
                screenshotDir = argv[++i];
            }
        }
        LocalFree(argv);
    }

    int exitCode = 0;

    if (!screenshotDir.empty())
    {
        exitCode = RunScreenshotDump(screenshotDir);
    }
    else if (silentMode)
    {
        std::wstring err;
        const bool ok = ExtractEmbeddedPayload(hInstance, gApp.vst3InstallDir, gApp.presetsInstallDir, true, nullptr, err);
        exitCode = ok ? 0 : 1;
    }
    else
    {
        const int screenW = GetSystemMetrics(SM_CXSCREEN);
        const int screenH = GetSystemMetrics(SM_CYSCREEN);
        gApp.winW = max(920, min(REF_W, static_cast<int>(std::lround(screenW * 0.62))));
        gApp.winH = static_cast<int>(std::lround(static_cast<double>(gApp.winW) * static_cast<double>(REF_H) / static_cast<double>(REF_W)));
        const int posX = (screenW - gApp.winW) / 2;
        const int posY = (screenH - gApp.winH) / 2;

        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = InstallerWndProc;
        wc.hInstance = hInstance;
        wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_INSTALLER_ICON));
        wc.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_INSTALLER_ICON));
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"DAProductionInstallerWindowClass";
        RegisterClassExW(&wc);

        HWND hwnd = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_APPWINDOW,
            wc.lpszClassName,
            L"D.A Production Installer",
            WS_POPUP | WS_MINIMIZEBOX | WS_SYSMENU,
            posX, posY, gApp.winW, gApp.winH,
            nullptr, nullptr, hInstance, nullptr);

        gApp.hwnd = hwnd;
        UpdateLayeredFrame(hwnd);
        ShowWindow(hwnd, nCmdShow);
        UpdateWindow(hwnd);

        MSG msg{};
        while (GetMessageW(&msg, nullptr, 0, 0) > 0)
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        exitCode = static_cast<int>(msg.wParam);
    }

    gApp.welcomeBmp.reset();
    gApp.baseBmp.reset();
    Gdiplus::GdiplusShutdown(gdiplusToken);
    CoUninitialize();
    return exitCode;
}
