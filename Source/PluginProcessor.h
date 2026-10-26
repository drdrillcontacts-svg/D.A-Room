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
    double getTailLengthSeconds() const override { return 15.0; }

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

    // Persistent preset & A/B comparison state (survives GUI open/close and DAW project save/load)
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
            buffer.assign (static_cast<size_t> (juce::jmax (4, maxDelaySamples + 4)), 0.0f);
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

    juce::Reverb reverb;
    juce::Reverb::Parameters reverbParams;

    std::array<AllpassSection, 4> diffuserL, diffuserR;
    juce::AudioBuffer<float> predelayBuffer;
    juce::AudioBuffer<float> modulationBuffer;
    int predelayWritePosition = 0;
    int modulationWritePosition = 0;
    double currentSampleRate = 44100.0;
    float modulationPhase = 0.0f;

    juce::LinearSmoothedValue<float> wetSmoother;
    juce::LinearSmoothedValue<float> drySmoother;
    juce::LinearSmoothedValue<float> predelaySamplesSmoother;
    juce::LinearSmoothedValue<float> widthSmoother;

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
