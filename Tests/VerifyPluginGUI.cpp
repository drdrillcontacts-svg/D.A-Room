#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>

namespace
{
struct ModeMetrics
{
    double wetGainDb = -100.0;
    double rt60Sec   = 0.0;
    double c50Db     = 0.0;
    double lowHiDb   = 0.0;
};

bool verifyReverbModesDSP()
{
    constexpr double sr = 48000.0;
    constexpr int blockSize = 256;
    constexpr int totalSeconds = 6;
    constexpr int totalSamples = static_cast<int> (sr) * totalSeconds;
    const char* modeNames[5] = { "ROOM", "HALL", "PLATE", "CHAMBER", "AMBIENCE" };

    ModeMetrics metrics[5];

    std::cout << "\n--- D.A Room 5-Mode Acoustic DSP Verification (48 kHz) ---\n";
    for (int mode = 0; mode < 5; ++mode)
    {
        DARoomAudioProcessor proc;
        proc.applyPreset (100.0f, 0.0f, 3.5f, 78.0f, mode, 82.0f, 18.0f, 50.0f, 78.0f, 20.0f, 20000.0f);
        proc.prepareToPlay (sr, blockSize);

        std::vector<float> irL (static_cast<size_t> (totalSamples), 0.0f);
        std::vector<float> irR (static_cast<size_t> (totalSamples), 0.0f);

        juce::AudioBuffer<float> block (2, blockSize);
        juce::MidiBuffer midi;

        int pos = 0;
        while (pos < totalSamples)
        {
            const int n = juce::jmin (blockSize, totalSamples - pos);
            block.clear();
            if (pos == 0)
            {
                block.setSample (0, 0, 1.0f);
                block.setSample (1, 0, 1.0f);
            }

            juce::AudioBuffer<float> sub (block.getArrayOfWritePointers(), 2, 0, n);
            proc.processBlock (sub, midi);

            for (int i = 0; i < n; ++i)
            {
                const float l = sub.getSample (0, i);
                const float r = sub.getSample (1, i);
                if (! std::isfinite (l) || ! std::isfinite (r))
                {
                    std::cerr << "FAIL: Non-finite audio sample in mode " << modeNames[mode] << "\n";
                    return false;
                }
                irL[static_cast<size_t> (pos + i)] = l;
                irR[static_cast<size_t> (pos + i)] = r;
            }
            pos += n;
        }
        proc.releaseResources();

        std::vector<double> schroeder (static_cast<size_t> (totalSamples + 1), 0.0);
        for (int i = totalSamples - 1; i >= 0; --i)
        {
            const double l = irL[static_cast<size_t> (i)];
            const double r = irR[static_cast<size_t> (i)];
            schroeder[static_cast<size_t> (i)] = schroeder[static_cast<size_t> (i + 1)] + 0.5 * (l * l + r * r);
        }

        const double totalE = schroeder[0];
        if (totalE < 1e-6)
        {
            std::cerr << "FAIL: Silent output in mode " << modeNames[mode] << "\n";
            return false;
        }

        int i5 = 0, i25 = totalSamples - 1;
        const int n50 = static_cast<int> (0.050 * sr);
        double earlyE = 0.0, lateE = 0.0, lowE = 0.0, hiE = 0.0;
        float lp350 = 0.0f, hp3k = 0.0f;
        const float a350 = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 350.0f / static_cast<float> (sr));
        const float a3k  = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 3200.0f / static_cast<float> (sr));

        for (int i = 0; i < totalSamples; ++i)
        {
            const double db = 10.0 * std::log10 (juce::jmax (1e-18, schroeder[static_cast<size_t> (i)] / totalE));
            if (db >= -5.0)
                i5 = i;
            if (db >= -25.0)
                i25 = i;

            const float l = irL[static_cast<size_t> (i)];
            const float r = irR[static_cast<size_t> (i)];
            const double e = 0.5 * (static_cast<double> (l) * l + static_cast<double> (r) * r);
            if (i < n50)
                earlyE += e;
            else
                lateE += e;

            const float m = 0.5f * (l + r);
            lp350 += a350 * (m - lp350);
            hp3k  += a3k  * (m - hp3k);
            const float hi = m - hp3k;
            if (i > n50)
            {
                lowE += static_cast<double> (lp350) * lp350;
                hiE  += static_cast<double> (hi) * hi;
            }
        }

        metrics[mode].wetGainDb = 10.0 * std::log10 (totalE);
        metrics[mode].rt60Sec   = 3.0 * static_cast<double> (i25 - i5) / sr;
        metrics[mode].c50Db     = 10.0 * std::log10 (earlyE / juce::jmax (1e-12, lateE));
        metrics[mode].lowHiDb   = 10.0 * std::log10 (lowE / juce::jmax (1e-12, hiE));

        std::cout << "  " << std::left << std::setw (9) << modeNames[mode] << std::right << std::fixed << std::setprecision (2)
                  << " | WetGain=" << std::setw (6) << metrics[mode].wetGainDb << " dB"
                  << " | RT60=" << std::setw (5) << metrics[mode].rt60Sec << " s"
                  << " | C50=" << std::setw (6) << metrics[mode].c50Db << " dB"
                  << " | Bass/Treble=" << std::setw (6) << metrics[mode].lowHiDb << " dB\n";

        if (metrics[mode].wetGainDb < -5.0 || metrics[mode].wetGainDb > 4.0)
        {
            std::cerr << "FAIL: Wet power gain out of studio calibration range in mode " << modeNames[mode] << "\n";
            return false;
        }
    }

    // Verify clear physical differentiation across modes
    if (! (metrics[0].rt60Sec < metrics[2].rt60Sec && metrics[2].rt60Sec < metrics[1].rt60Sec && metrics[1].rt60Sec < metrics[4].rt60Sec))
    {
        std::cerr << "FAIL: Mode RT60 hierarchy check failed (expected ROOM < PLATE < HALL < AMBIENCE)\n";
        return false;
    }
    if (metrics[0].c50Db - metrics[1].c50Db < 6.0 || metrics[0].c50Db - metrics[4].c50Db < 10.0)
    {
        std::cerr << "FAIL: Early-to-late spatial clarity differentiation too small\n";
        return false;
    }
    if (metrics[3].lowHiDb - metrics[2].lowHiDb < 3.5)
    {
        std::cerr << "FAIL: Chamber vs Plate tonal tilt differentiation too small\n";
        return false;
    }

    std::cout << "--- All 5 Reverb Modes Verified Distinct & Calibrated ---\n\n";
    return true;
}
} // namespace

int main (int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: VerifyPluginGUI <output_png_path>\n";
        return 2;
    }

    const juce::ScopedJuceInitialiser_GUI guiInit;

    if (! verifyReverbModesDSP())
        return 1;

    DARoomAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    if (editor == nullptr)
    {
        std::cerr << "FAIL: createEditor() returned nullptr\n";
        return 1;
    }

    if (editor->getWidth() != 1015 || editor->getHeight() != 388)
    {
        std::cerr << "FAIL: unexpected editor dimensions "
                  << editor->getWidth() << "x" << editor->getHeight() << "\n";
        return 1;
    }

   #if JUCE_WINDOWS
    editor->addToDesktop (0);
    editor->setTopLeftPosition (40, 40);
    editor->setVisible (true);

    if (auto* peer = editor->getPeer())
    {
        peer->performAnyPendingRepaintsNow();
        std::cout << "Active rendering engine index: " << peer->getCurrentRenderingEngine()
                  << " (" << peer->getAvailableRenderingEngines()[peer->getCurrentRenderingEngine()] << ")\n";
        if (peer->getCurrentRenderingEngine() != 0)
        {
            std::cerr << "FAIL: Windows rendering engine is not 0 (Software Renderer)\n";
            return 1;
        }
    }
   #endif

    // Capture 2x supersampled snapshot (2030x776) so we can inspect the mode selector text at native resolution
    const juce::Image snapshot = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
    if (! snapshot.isValid())
    {
        std::cerr << "FAIL: createComponentSnapshot returned invalid image\n";
        return 1;
    }

    const juce::Colour archTop = snapshot.getPixelAt (507 * 2, 88 * 2);
    const juce::Colour archGlow = snapshot.getPixelAt (452 * 2, 145 * 2);

    std::cout << "Sampled arch_top RGB=("
              << static_cast<int> (archTop.getRed()) << ","
              << static_cast<int> (archTop.getGreen()) << ","
              << static_cast<int> (archTop.getBlue()) << ")\n";
    std::cout << "Sampled arch_glow RGB=("
              << static_cast<int> (archGlow.getRed()) << ","
              << static_cast<int> (archGlow.getGreen()) << ","
              << static_cast<int> (archGlow.getBlue()) << ")\n";

    if (archTop.getBlue() < 150 || archTop.getGreen() < 140 || archGlow.getBlue() < 55)
    {
        std::cerr << "FAIL: Background GUI artwork did not render (pixels are dark/black)!\n";
        return 1;
    }

    const juce::File outFile (juce::String::fromUTF8 (argv[1]));
    outFile.deleteFile();
    {
        juce::FileOutputStream fos (outFile);
        if (! fos.openedOk())
        {
            std::cerr << "FAIL: Could not open output file " << outFile.getFullPathName() << "\n";
            return 1;
        }
        juce::PNGImageFormat png;
        if (! png.writeImageToStream (snapshot, fos))
        {
            std::cerr << "FAIL: Could not write PNG snapshot\n";
            return 1;
        }
    }

    editor->removeFromDesktop();
    editor.reset();
    processor.releaseResources();

    std::cout << "SUCCESS: Verified D.A Room DSP & GUI ("
              << snapshot.getWidth() << "x" << snapshot.getHeight() << ") -> "
              << outFile.getFullPathName() << "\n";
    return 0;
}
