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
#include <compressapi.h>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#pragma comment(lib, "cabinet.lib")

namespace fs = std::filesystem;

namespace
{
struct Entry
{
    uint8_t targetRoot; // 0 = VST3 root, 1 = Presets root
    std::string relPathUtf8;
    std::vector<uint8_t> data;
};

void appendBytes(std::vector<uint8_t>& out, const void* src, size_t len)
{
    const auto* p = static_cast<const uint8_t*>(src);
    out.insert(out.end(), p, p + len);
}

template <typename T>
void appendPod(std::vector<uint8_t>& out, const T& val)
{
    appendBytes(out, &val, sizeof(T));
}

bool readAllBytes(const fs::path& p, std::vector<uint8_t>& out)
{
    std::ifstream ifs(p, std::ios::binary | std::ios::ate);
    if (!ifs)
        return false;
    const auto sz = ifs.tellg();
    if (sz < 0)
        return false;
    out.resize(static_cast<size_t>(sz));
    ifs.seekg(0, std::ios::beg);
    if (sz > 0 && !ifs.read(reinterpret_cast<char*>(out.data()), sz))
        return false;
    return true;
}

std::string toForwardSlashUtf8(const fs::path& p)
{
    const auto u8 = p.generic_u8string();
    return std::string(u8.begin(), u8.end());
}
} // namespace

int wmain(int argc, wchar_t* argv[])
{
    if (argc < 4)
    {
        std::fwprintf(stderr, L"Usage: PackPayload <vst3_bundle_dir> <presets_dir> <out_payload.bin>\n");
        return 1;
    }

    const fs::path vst3Dir(argv[1]);
    const fs::path presetsDir(argv[2]);
    const fs::path outFile(argv[3]);

    if (!fs::exists(vst3Dir))
    {
        std::fwprintf(stderr, L"Error: VST3 path does not exist: %ls\n", vst3Dir.c_str());
        return 2;
    }

    std::vector<Entry> entries;
    const fs::path vst3Parent = vst3Dir.parent_path();

    if (fs::is_directory(vst3Dir))
    {
        for (const auto& dirEntry : fs::recursive_directory_iterator(vst3Dir))
        {
            if (!dirEntry.is_regular_file())
                continue;
            Entry e{};
            e.targetRoot = 0;
            e.relPathUtf8 = toForwardSlashUtf8(fs::relative(dirEntry.path(), vst3Parent));
            if (!readAllBytes(dirEntry.path(), e.data))
            {
                std::fwprintf(stderr, L"Failed to read file: %ls\n", dirEntry.path().c_str());
                return 3;
            }
            entries.push_back(std::move(e));
        }
    }
    else if (fs::is_regular_file(vst3Dir))
    {
        Entry e{};
        e.targetRoot = 0;
        e.relPathUtf8 = toForwardSlashUtf8(vst3Dir.filename());
        if (!readAllBytes(vst3Dir, e.data))
            return 3;
        entries.push_back(std::move(e));
    }

    if (fs::exists(presetsDir) && fs::is_directory(presetsDir))
    {
        for (const auto& dirEntry : fs::recursive_directory_iterator(presetsDir))
        {
            if (!dirEntry.is_regular_file())
                continue;
            Entry e{};
            e.targetRoot = 1;
            e.relPathUtf8 = toForwardSlashUtf8(fs::relative(dirEntry.path(), presetsDir));
            if (!readAllBytes(dirEntry.path(), e.data))
            {
                std::fwprintf(stderr, L"Failed to read preset: %ls\n", dirEntry.path().c_str());
                return 4;
            }
            entries.push_back(std::move(e));
        }
    }

    if (entries.empty())
    {
        std::fwprintf(stderr, L"Error: No files found to pack.\n");
        return 5;
    }

    std::vector<uint8_t> rawArchive;
    rawArchive.reserve(16 * 1024 * 1024);

    const char magic[4] = { 'D', 'A', 'R', '1' };
    appendBytes(rawArchive, magic, 4);
    const uint32_t count = static_cast<uint32_t>(entries.size());
    appendPod(rawArchive, count);

    for (const auto& e : entries)
    {
        appendPod(rawArchive, e.targetRoot);
        const uint16_t pathLen = static_cast<uint16_t>(e.relPathUtf8.size());
        appendPod(rawArchive, pathLen);
        appendBytes(rawArchive, e.relPathUtf8.data(), pathLen);
        const uint64_t dataLen = static_cast<uint64_t>(e.data.size());
        appendPod(rawArchive, dataLen);
        if (dataLen > 0)
            appendBytes(rawArchive, e.data.data(), e.data.size());
    }

    COMPRESSOR_HANDLE compressor = nullptr;
    if (!CreateCompressor(COMPRESS_ALGORITHM_LZMS, nullptr, &compressor))
    {
        std::fwprintf(stderr, L"CreateCompressor failed: %lu\n", GetLastError());
        return 6;
    }

    SIZE_T compressedSize = 0;
    Compress(compressor, rawArchive.data(), rawArchive.size(), nullptr, 0, &compressedSize);
    std::vector<uint8_t> compressed(compressedSize);
    if (!Compress(compressor, rawArchive.data(), rawArchive.size(),
                  compressed.data(), compressed.size(), &compressedSize))
    {
        std::fwprintf(stderr, L"Compress failed: %lu\n", GetLastError());
        CloseCompressor(compressor);
        return 7;
    }
    CloseCompressor(compressor);
    compressed.resize(compressedSize);

    fs::create_directories(outFile.parent_path());
    std::ofstream ofs(outFile, std::ios::binary | std::ios::trunc);
    if (!ofs)
    {
        std::fwprintf(stderr, L"Failed to open output payload: %ls\n", outFile.c_str());
        return 8;
    }

    const char outerMagic[4] = { 'D', 'A', 'L', 'Z' };
    const uint64_t uncompressedLen = static_cast<uint64_t>(rawArchive.size());
    ofs.write(outerMagic, 4);
    ofs.write(reinterpret_cast<const char*>(&uncompressedLen), sizeof(uncompressedLen));
    ofs.write(reinterpret_cast<const char*>(compressed.data()), static_cast<std::streamsize>(compressed.size()));
    ofs.close();

    std::wprintf(L"Packed %u files: %llu bytes -> %llu bytes (%ls)\n",
                 count,
                 static_cast<unsigned long long>(rawArchive.size()),
                 static_cast<unsigned long long>(compressed.size()),
                 outFile.c_str());
    return 0;
}
