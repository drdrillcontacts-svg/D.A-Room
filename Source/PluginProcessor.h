#pragma once
#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include <array>

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
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState parameters;
    void applyPreset (float mixPercent, float predelayMs, float decaySeconds, float sizePercent,
                      int modeIndex, float diffusion, float modulation, float damping,
                      float width, float lowCutHz, float highCutHz);
    void setMode (int modeIndex);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::dsp::Reverb reverb;
    juce::dsp::Reverb::Parameters reverbParams;
    juce::AudioBuffer<float> predelayBuffer, modulationBuffer;
    int predelayWritePosition = 0, modulationWritePosition = 0;
    double currentSampleRate = 44100.0;
    float modulationPhase = 0.0f;
    juce::LinearSmoothedValue<float> wetSmoother, drySmoother;
    juce::dsp::IIR::Filter<float> lowCutL, lowCutR, highCutL, highCutR;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DARoomAudioProcessor)
};
