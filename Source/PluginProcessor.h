#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include "FactoryPresets.h"

class DARoomAudioProcessor final : public juce::AudioProcessor
{
public:
    DARoomAudioProcessor();
    ~DARoomAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 18.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState parameters;

    void applyPreset (float mixPercent, float predelayMs, float decaySeconds, float sizePercent,
                      int modeIndex, float diffusion, float modulation, float damping,
                      float width, float lowCutHz, float highCutHz);
    void applyFactoryPreset (int index);
    void setMode (int modeIndex);
    int getMode() const noexcept;

    int getSelectedPresetIndex() const noexcept { return currentPresetIndex; }
    juce::String getCurrentPresetDisplayName() const;
    int getActiveAB() const noexcept { return activeAB; }
    void switchAB (int targetSlot);
    void copyActiveABToOther();

    bool loadPresetFromFile (const juce::File& file);
    bool savePresetToFile (const juce::File& file, const juce::String& customName = {});

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static void setParameterValueWithGesture (juce::RangedAudioParameter* param, float plainValue);

    void captureSnapshot (std::array<float, 11>& dest) const;
    void restoreSnapshot (const std::array<float, 11>& src);

    struct AllpassSection
    {
        std::vector<float> buffer;
        int writePos = 0;

        void prepare (int maxDelaySamples)
        {
            buffer.assign (static_cast<size_t> (juce::jmax (8, maxDelaySamples + 8)), 0.0f);
            writePos = 0;
        }

        void reset() noexcept
        {
            std::fill (buffer.begin(), buffer.end(), 0.0f);
            writePos = 0;
        }

        float process (float input, int delaySamples, float feedback) noexcept
        {
            const int len = static_cast<int> (buffer.size());
            if (len < 4)
                return input;

            const int d = juce::jlimit (1, len - 2, delaySamples);
            int readPos = writePos - d;
            if (readPos < 0)
                readPos += len;

            const float delayed = buffer[static_cast<size_t> (readPos)];
            const float v = input - feedback * delayed;
            buffer[static_cast<size_t> (writePos)] = v;

            if (++writePos >= len)
                writePos = 0;

            return delayed + feedback * v;
        }
    };

    struct FDNDelayLine
    {
        std::vector<float> buffer;
        int writePos = 0;
        float smoothedDelay = 100.0f;
        float hfLpState = 0.0f;
        float lfLpState = 0.0f;
        float dcX1 = 0.0f;
        float dcY1 = 0.0f;

        void prepare (int maxDelaySamples)
        {
            buffer.assign (static_cast<size_t> (juce::jmax (16, maxDelaySamples + 16)), 0.0f);
            writePos = 0;
            hfLpState = 0.0f;
            lfLpState = 0.0f;
            dcX1 = 0.0f;
            dcY1 = 0.0f;
        }

        void reset() noexcept
        {
            std::fill (buffer.begin(), buffer.end(), 0.0f);
            writePos = 0;
            hfLpState = 0.0f;
            lfLpState = 0.0f;
            dcX1 = 0.0f;
            dcY1 = 0.0f;
        }

        float readFractional (float delaySamples) const noexcept
        {
            const int len = static_cast<int> (buffer.size());
            const float clamped = juce::jlimit (2.0f, static_cast<float> (len - 4), delaySamples);
            const int dInt = static_cast<int> (clamped);
            const float frac = clamped - static_cast<float> (dInt);

            int r0 = writePos - dInt;
            if (r0 < 0)
                r0 += len;
            int r1 = r0 - 1;
            if (r1 < 0)
                r1 += len;

            return buffer[static_cast<size_t> (r0)] * (1.0f - frac)
                 + buffer[static_cast<size_t> (r1)] * frac;
        }

        void writeSample (float x) noexcept
        {
            const int len = static_cast<int> (buffer.size());
            buffer[static_cast<size_t> (writePos)] = x;
            if (++writePos >= len)
                writePos = 0;
        }
    };

    // Dual-grain Hann-windowed octave-up harmonic air generator (used in Ambience mode)
    struct ShimmerOctaver
    {
        std::vector<float> bufL, bufR;
        int writePos = 0;
        double phase = 0.0;
        int grainLen = 2048;
        float lpL = 0.0f, lpR = 0.0f;
        float hpX1L = 0.0f, hpY1L = 0.0f, hpX1R = 0.0f, hpY1R = 0.0f;

        void prepare (double sampleRate)
        {
            grainLen = juce::jmax (256, static_cast<int> (std::round (sampleRate * 0.048)));
            const int cap = grainLen * 4 + 16;
            bufL.assign (static_cast<size_t> (cap), 0.0f);
            bufR.assign (static_cast<size_t> (cap), 0.0f);
            writePos = 0;
            phase = 0.0;
            lpL = lpR = 0.0f;
            hpX1L = hpY1L = hpX1R = hpY1R = 0.0f;
        }

        void reset() noexcept
        {
            std::fill (bufL.begin(), bufL.end(), 0.0f);
            std::fill (bufR.begin(), bufR.end(), 0.0f);
            writePos = 0;
            phase = 0.0;
            lpL = lpR = 0.0f;
            hpX1L = hpY1L = hpX1R = hpY1R = 0.0f;
        }

        void process (float inL, float inR, float lpCoeff, float& outL, float& outR) noexcept
        {
            const int len = static_cast<int> (bufL.size());
            if (len < 32)
            {
                outL = outR = 0.0f;
                return;
            }

            bufL[static_cast<size_t> (writePos)] = inL;
            bufR[static_cast<size_t> (writePos)] = inR;

            auto sampleGrain = [&] (double p, float& gL, float& gR) noexcept
            {
                // Pitch +12st (2x speed): delay sweeps from grainLen down to 0 as p goes 0 -> 1
                const double delay = (1.0 - p) * static_cast<double> (grainLen) + 4.0;
                const int dInt = static_cast<int> (delay);
                const float frac = static_cast<float> (delay - static_cast<double> (dInt));
                int r0 = writePos - dInt;
                while (r0 < 0)
                    r0 += len;
                int r1 = r0 - 1;
                if (r1 < 0)
                    r1 += len;

                const float w = 0.5f - 0.5f * std::cos (static_cast<float> (p * juce::MathConstants<double>::twoPi));
                gL = (bufL[static_cast<size_t> (r0)] * (1.0f - frac) + bufL[static_cast<size_t> (r1)] * frac) * w;
                gR = (bufR[static_cast<size_t> (r0)] * (1.0f - frac) + bufR[static_cast<size_t> (r1)] * frac) * w;
            };

            const double pA = phase;
            const double pB = (phase >= 0.5) ? (phase - 0.5) : (phase + 0.5);
            float aL = 0.0f, aR = 0.0f, bL = 0.0f, bR = 0.0f;
            sampleGrain (pA, aL, aR);
            sampleGrain (pB, bL, bR);

            if (++writePos >= len)
                writePos = 0;

            phase += 1.0 / static_cast<double> (grainLen);
            if (phase >= 1.0)
                phase -= 1.0;

            float sL = aL + bL;
            float sR = aR + bR;

            // Gentle highpass (~500 Hz) + silky lowpass (~4 kHz) for smooth crystalline halo
            const float hpL = 0.94f * (hpY1L + sL - hpX1L);
            hpX1L = sL;
            hpY1L = hpL;
            const float hpR = 0.94f * (hpY1R + sR - hpX1R);
            hpX1R = sR;
            hpY1R = hpR;

            lpL += lpCoeff * (hpL - lpL);
            lpR += lpCoeff * (hpR - lpR);
            outL = lpL;
            outR = lpR;
        }
    };

    static constexpr size_t kNumFDNLines = 8;

    std::array<FDNDelayLine, kNumFDNLines> fdnLines;
    std::array<AllpassSection, kNumFDNLines> tankAllpasses;
    std::array<AllpassSection, 4> diffuserL, diffuserR;

    juce::AudioBuffer<float> predelayBuffer;
    juce::AudioBuffer<float> erBuffer; // Stereo Early Reflections & late-offset delay line
    int predelayWritePosition = 0;
    int erWritePosition = 0;

    ShimmerOctaver shimmer;

    double currentSampleRate = 48000.0;
    double lfoPhase = 0.0;

    // Input voicing states per mode
    float inHpStateL = 0.0f, inHpStateR = 0.0f;
    float inLpStateL = 0.0f, inLpStateR = 0.0f;

    juce::LinearSmoothedValue<float> wetSmoother;
    juce::LinearSmoothedValue<float> drySmoother;
    juce::LinearSmoothedValue<float> predelaySamplesSmoother;
    juce::LinearSmoothedValue<float> widthSmoother;
    juce::LinearSmoothedValue<float> erGainSmoother;
    juce::LinearSmoothedValue<float> lateGainSmoother;
    juce::LinearSmoothedValue<float> shimmerGainSmoother;

    juce::dsp::StateVariableTPTFilter<float> lowCutFilter;
    juce::dsp::StateVariableTPTFilter<float> highCutFilter;
    float lastLowCutHz = -1.0f;
    float lastHighCutHz = -1.0f;

    int currentPresetIndex = DARoomPresets::defaultPresetIndex;
    juce::String customLoadedPresetName;
    int activeAB = 0;
    std::array<float, 11> snapshotA {};
    std::array<float, 11> snapshotB {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DARoomAudioProcessor)
};
