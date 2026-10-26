#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <array>
#include <memory>

class DARoomLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    DARoomLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};

class DARoomAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                         private juce::Timer
{
public:
    explicit DARoomAudioProcessorEditor (DARoomAudioProcessor&);
    ~DARoomAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;
    void visibilityChanged() override;

private:
    static constexpr int referenceWidth = 2030;
    static constexpr int referenceHeight = 775;

    static juce::Image loadSoftwareARGBImage (const void* data, size_t sizeBytes);
    void enforceSoftwareRendererOnWindows();
    void ensureScaledBackground();

    void timerCallback() override;
    void syncUIFromProcessor();
    void updateValueLabels();
    void updateModeButtons (int modeIndex);
    void updateABButtons (int abSlot);
    void openPresetMenu();
    void stepPreset (int delta);
    void openSettingsMenu();
    void promptLoadPreset();
    void promptSavePreset();

    juce::Rectangle<int> scaledBounds (float x, float y, float w, float h) const;
    juce::Rectangle<float> scaledFloatBounds (float x, float y, float w, float h) const;

    DARoomAudioProcessor& audioProcessor;
    juce::Image background;
    juce::Image scaledBackground;
    DARoomLookAndFeel customLookAndFeel;

    // 10 rotary knobs (4 main + 4 bottom + 2 filter knobs) + 2 linear filter track sliders
    juce::Slider mixSlider, predelaySlider, decaySlider, sizeSlider;
    juce::Slider diffusionSlider, modulationSlider, dampingSlider, widthSlider;
    juce::Slider lowCutKnob, highCutKnob;
    juce::Slider lowCutTrackSlider, highCutTrackSlider;

    std::array<juce::Slider*, 10> primarySliders {{
        &mixSlider, &predelaySlider, &decaySlider, &sizeSlider,
        &diffusionSlider, &modulationSlider, &dampingSlider, &widthSlider,
        &lowCutKnob, &highCutKnob
    }};

    std::array<juce::Label, 10> values;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 10> primaryAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lowCutTrackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> highCutTrackAttachment;

    juce::TextButton prevPresetButton { "<" };
    juce::TextButton presetTitleButton { "Deep Chamber" };
    juce::TextButton nextPresetButton { ">" };

    std::array<juce::TextButton, 5> modeButtons;
    std::array<juce::TextButton, 2> abButtons;
    juce::TextButton settingsButton { "" };

    std::unique_ptr<juce::FileChooser> fileChooser;
    int cachedMode = -1;
    int cachedAB = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DARoomAudioProcessorEditor)
};
