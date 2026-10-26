#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <array>

class DARoomLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;
};

class DARoomAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit DARoomAudioProcessorEditor (DARoomAudioProcessor&);
    ~DARoomAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    static constexpr int referenceWidth = 2030;
    static constexpr int referenceHeight = 775;
    DARoomAudioProcessor& audioProcessor;
    juce::Image background;
    DARoomLookAndFeel customLookAndFeel;

    juce::Slider mix, predelay, decay, size, diffusion, modulation, damping, width, lowCut, highCut;
    std::array<juce::Slider*, 10> sliders {{ &mix, &predelay, &decay, &size, &diffusion,
                                             &modulation, &damping, &width, &lowCut, &highCut }};
    std::array<juce::Label, 10> values;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 10> attachments;
    juce::ComboBox presetSelector;
    std::array<juce::TextButton, 5> modeButtons;
    std::array<juce::TextButton, 2> abButtons;
    int activeMode = 3;
    std::array<float, 11> snapshotA {}, snapshotB {};
    bool hasSnapshotA = false, hasSnapshotB = false;
    int activeAB = 0;
    void captureSnapshot (std::array<float, 11>&);
    void restoreSnapshot (const std::array<float, 11>&);

    void styleRotary (juce::Slider&, const juce::String&);
    void styleValue (juce::Label&);
    void updateValueLabels();
    void updateModeButtons();
    juce::Rectangle<int> scaledBounds (float x, float y, float w, float h) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DARoomAudioProcessorEditor)
};
