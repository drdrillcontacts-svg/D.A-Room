#include "PluginEditor.h"
#include "BinaryData.h"
#include <cmath>

DARoomLookAndFeel::DARoomLookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xf407131c));
    setColour (juce::PopupMenu::textColourId, juce::Colour (0xffdcf3ff));
    setColour (juce::PopupMenu::headerTextColourId, juce::Colour (0xff67c9ec));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff103a4d));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colour (0xffffffff));
}

void DARoomLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                          float pos, float start, float end, juce::Slider& slider)
{
    const float minDim = static_cast<float> (juce::jmin (w, h));
    const float cx = static_cast<float> (x) + static_cast<float> (w) * 0.5f;
    const float cy = static_cast<float> (y) + static_cast<float> (h) * 0.5f;
    const float outerR = minDim * 0.495f;

    // Cover static baked-in knob artwork cleanly inside the recessed bezel
    const auto outerBounds = juce::Rectangle<float> (cx - outerR, cy - outerR, outerR * 2.0f, outerR * 2.0f);
    g.setColour (juce::Colour (0xff050c12));
    g.fillEllipse (outerBounds);

    const float arcR = outerR * 0.885f;
    const float capR = outerR * 0.755f;
    const float angle = start + pos * (end - start);

    // Subtle unlit track arc
    juce::Path trackPath;
    trackPath.addCentredArc (cx, cy, arcR, arcR, 0.0f, start, end, true);
    g.setColour (juce::Colour (0xff1a2d38));
    g.strokePath (trackPath, juce::PathStrokeType (juce::jmax (1.6f, minDim * 0.028f),
                                                   juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

    // Active cyan glow + core arc
    if (pos > 0.002f)
    {
        juce::Path valueArc;
        valueArc.addCentredArc (cx, cy, arcR, arcR, 0.0f, start, angle, true);

        g.setColour (juce::Colour (0x444ad2ff));
        g.strokePath (valueArc, juce::PathStrokeType (juce::jmax (3.5f, minDim * 0.068f),
                                                      juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));

        g.setColour (slider.isMouseOverOrDragging() ? juce::Colour (0xffa8edff) : juce::Colour (0xff7fdfff));
        g.strokePath (valueArc, juce::PathStrokeType (juce::jmax (2.0f, minDim * 0.034f),
                                                      juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
    }

    // Dark brushed metallic cap
    const auto capBounds = juce::Rectangle<float> (cx - capR, cy - capR, capR * 2.0f, capR * 2.0f);
    g.setColour (juce::Colour (0xaa000000));
    g.fillEllipse (capBounds.expanded (1.5f));

    juce::ColourGradient capGrad (juce::Colour (0xff1c2d37), cx - capR * 0.3f, cy - capR,
                                  juce::Colour (0xff050b10), cx + capR * 0.3f, cy + capR, false);
    g.setGradientFill (capGrad);
    g.fillEllipse (capBounds);

    g.setColour (slider.isMouseOverOrDragging() ? juce::Colour (0xff42758a) : juce::Colour (0xff27414d));
    g.drawEllipse (capBounds, 1.1f);

    // Crisp outer rim pointer tick (matching DAroom_GUI.png style)
    const float tickInner = capR * 0.56f;
    const float tickOuter = capR * 0.86f;
    const float sinA = std::sin (angle);
    const float cosA = std::cos (angle);
    g.setColour (juce::Colour (0xffeaf9ff));
    g.drawLine (cx + sinA * tickInner, cy - cosA * tickInner,
                cx + sinA * tickOuter, cy - cosA * tickOuter,
                juce::jmax (1.6f, minDim * 0.026f));
}

void DARoomLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                                          juce::Slider::SliderStyle /*style*/, juce::Slider& slider)
{
    const auto area = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                              static_cast<float> (width), static_cast<float> (height));
    const float cy = area.getCentreY();
    const float pad = juce::jmax (5.0f, area.getHeight() * 0.32f);
    const float x0 = area.getX() + pad;
    const float x1 = area.getRight() - pad;
    const float thumbX = juce::jlimit (x0, x1, sliderPos);

    // Unlit track
    g.setColour (juce::Colour (0xff1c303b));
    g.drawLine (x0, cy, x1, cy, 2.2f);

    // Lit cyan track + soft glow
    g.setColour (juce::Colour (0x444ad2ff));
    g.drawLine (x0, cy, thumbX, cy, 5.2f);
    g.setColour (juce::Colour (0xff78dcff));
    g.drawLine (x0, cy, thumbX, cy, 2.4f);

    // Glowing thumb
    const float r = juce::jmax (3.6f, area.getHeight() * 0.25f);
    g.setColour (juce::Colour (0x556ce0ff));
    g.fillEllipse (thumbX - r * 1.7f, cy - r * 1.7f, r * 3.4f, r * 3.4f);
    g.setColour (slider.isMouseOverOrDragging() ? juce::Colour (0xffffffff) : juce::Colour (0xffdff7ff));
    g.fillEllipse (thumbX - r, cy - r, r * 2.0f, r * 2.0f);
}

void DARoomLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                              const juce::Colour& /*backgroundColour*/,
                                              bool isHighlighted, bool isDown)
{
    const auto bounds = button.getLocalBounds().toFloat();
    const auto id = button.getComponentID();

    if (id == "transparentHit")
    {
        if (isHighlighted || isDown)
        {
            g.setColour (juce::Colour (isDown ? 0x3578dcff : 0x1e78dcff));
            g.fillRoundedRectangle (bounds.reduced (2.0f), 6.0f);
        }
        return;
    }

    if (id == "presetCenter")
    {
        if (isHighlighted || isDown)
        {
            g.setColour (juce::Colour (isDown ? 0x2c6ce0ff : 0x186ce0ff));
            g.fillRect (bounds.reduced (2.0f, 3.0f));
        }
        return;
    }

    if (id == "modeBtn")
    {
        if (button.getToggleState())
        {
            juce::ColourGradient activeGrad (juce::Colour (0x2416607d), bounds.getCentreX(), bounds.getY(),
                                             juce::Colour (0x6e14536e), bounds.getCentreX(), bounds.getBottom(), false);
            g.setGradientFill (activeGrad);
            g.fillRoundedRectangle (bounds.reduced (2.5f, 2.5f), 8.0f);
        }
        else if (isHighlighted)
        {
            g.setColour (juce::Colour (0x2056bce0));
            g.fillRoundedRectangle (bounds.reduced (2.5f, 2.5f), 8.0f);
        }
        return;
    }

    if (id == "abBtn")
    {
        if (button.getToggleState())
        {
            g.setColour (juce::Colour (0x98114054));
            g.fillRoundedRectangle (bounds.reduced (2.5f), 7.0f);
        }
        else if (isHighlighted)
        {
            g.setColour (juce::Colour (0x2456bce0));
            g.fillRoundedRectangle (bounds.reduced (2.5f), 7.0f);
        }
        return;
    }
}

void DARoomLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                        bool isHighlighted, bool /*isDown*/)
{
    const auto id = button.getComponentID();
    if (id == "transparentHit")
        return; // Uses artwork icon (<, >, gear) directly

    const auto bounds = button.getLocalBounds();
    const float fontH = juce::jmax (10.5f, static_cast<float> (bounds.getHeight()) * (id == "presetCenter" ? 0.50f : 0.42f));
    g.setFont (juce::Font (juce::FontOptions (fontH)));

    if (id == "presetCenter")
    {
        g.setColour (isHighlighted ? juce::Colour (0xffffffff) : juce::Colour (0xffeaf8ff));
        g.drawFittedText (button.getButtonText(), bounds.reduced (8, 2), juce::Justification::centred, 1);
        return;
    }

    const bool active = button.getToggleState();
    g.setColour (active ? juce::Colour (0xffb8f2ff)
                        : (isHighlighted ? juce::Colour (0xffd8f0fa) : juce::Colour (0xff9ab9c9)));
    g.drawFittedText (button.getButtonText(), bounds.reduced (4, 2), juce::Justification::centred, 1);
}

juce::Image DARoomAudioProcessorEditor::loadSoftwareARGBImage (const void* data, size_t sizeBytes)
{
    if (data == nullptr || sizeBytes == 0)
        return {};

    juce::MemoryInputStream memStream (data, sizeBytes, false);
    juce::PNGImageFormat pngFormat;
    juce::Image decoded = pngFormat.decodeImage (memStream);

    if (! decoded.isValid())
        decoded = juce::ImageFileFormat::loadFrom (data, sizeBytes);

    if (! decoded.isValid())
        return {};

    const int w = decoded.getWidth();
    const int h = decoded.getHeight();
    if (w <= 0 || h <= 0)
        return {};

    // Copy explicitly into CPU-backed SoftwareImageType (Image::ARGB, opaque alpha = 0xff)
    // so rendering never depends on pre-HWND Direct2D/DXGI GPU textures on Windows.
    juce::Image softImg (juce::Image::ARGB, w, h, true, juce::SoftwareImageType{});
    {
        const juce::Image::BitmapData src (decoded, juce::Image::BitmapData::readOnly);
        const juce::Image::BitmapData dst (softImg, juce::Image::BitmapData::readWrite);

        for (int y = 0; y < h; ++y)
        {
            auto* dLine = dst.getLinePointer (y);
            for (int x = 0; x < w; ++x)
            {
                const auto col = src.getPixelColour (x, y);
                reinterpret_cast<juce::PixelARGB*> (dLine + x * dst.pixelStride)
                    ->setARGB (0xff, col.getRed(), col.getGreen(), col.getBlue());
            }
        }
    }

    return softImg;
}

DARoomAudioProcessorEditor::DARoomAudioProcessorEditor (DARoomAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setOpaque (true);
    setLookAndFeel (&customLookAndFeel);
    background = loadSoftwareARGBImage (BinaryData::DAroom_GUI_png,
                                        static_cast<size_t> (BinaryData::DAroom_GUI_pngSize));

    const juce::StringArray tooltips {
        "Mix", "Predelay", "Decay", "Size",
        "Diffusion", "Modulation", "Damping", "Width",
        "Low Cut", "High Cut"
    };

    for (size_t i = 0; i < primarySliders.size(); ++i)
    {
        auto* s = primarySliders[i];
        s->setLookAndFeel (&customLookAndFeel);
        s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s->setTooltip (tooltips[static_cast<int> (i)]);
        s->setDoubleClickReturnValue (true, audioProcessor.parameters.getParameter (DARoomPresets::parameterIDs[i])
                                                ->convertFrom0to1 (audioProcessor.parameters.getParameter (DARoomPresets::parameterIDs[i])->getDefaultValue()));
        addAndMakeVisible (*s);

        values[i].setJustificationType (i >= 8 ? juce::Justification::centredLeft : juce::Justification::centred);
        values[i].setColour (juce::Label::textColourId, juce::Colour (0xffb8e4f7));
        values[i].setInterceptsMouseClicks (false, false);
        addAndMakeVisible (values[i]);

        primaryAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            audioProcessor.parameters, DARoomPresets::parameterIDs[i], *s);

        s->onValueChange = [this] { updateValueLabels(); };
    }

    for (auto* track : { &lowCutTrackSlider, &highCutTrackSlider })
    {
        track->setLookAndFeel (&customLookAndFeel);
        track->setSliderStyle (juce::Slider::LinearHorizontal);
        track->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (*track);
    }
    lowCutTrackSlider.setTooltip ("Low Cut");
    highCutTrackSlider.setTooltip ("High Cut");

    lowCutTrackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.parameters, "lowcut", lowCutTrackSlider);
    highCutTrackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.parameters, "highcut", highCutTrackSlider);

    lowCutTrackSlider.onValueChange  = [this] { updateValueLabels(); };
    highCutTrackSlider.onValueChange = [this] { updateValueLabels(); };

    // Top Preset Bar (< | Preset Name | >)
    prevPresetButton.setComponentID ("transparentHit");
    prevPresetButton.setTooltip ("Previous Preset");
    prevPresetButton.onClick = [this] { stepPreset (-1); };
    addAndMakeVisible (prevPresetButton);

    presetTitleButton.setComponentID ("presetCenter");
    presetTitleButton.setTooltip ("Browse Factory Presets");
    presetTitleButton.onClick = [this] { openPresetMenu(); };
    addAndMakeVisible (presetTitleButton);

    nextPresetButton.setComponentID ("transparentHit");
    nextPresetButton.setTooltip ("Next Preset");
    nextPresetButton.onClick = [this] { stepPreset (1); };
    addAndMakeVisible (nextPresetButton);

    // Direct-click Reverb Mode Pill Buttons
    for (size_t i = 0; i < modeButtons.size(); ++i)
    {
        modeButtons[i].setComponentID ("modeBtn");
        modeButtons[i].setButtonText (DARoomPresets::modeDisplayNames[i]);
        modeButtons[i].setClickingTogglesState (false);
        const int modeIdx = static_cast<int> (i);
        modeButtons[i].onClick = [this, modeIdx]
        {
            audioProcessor.setMode (modeIdx);
            syncUIFromProcessor();
        };
        addAndMakeVisible (modeButtons[i]);
    }

    // A / B Comparison Capsule
    abButtons[0].setButtonText ("A");
    abButtons[1].setButtonText ("B");
    for (size_t i = 0; i < abButtons.size(); ++i)
    {
        abButtons[i].setComponentID ("abBtn");
        abButtons[i].setClickingTogglesState (false);
        const int slot = static_cast<int> (i);
        abButtons[i].onClick = [this, slot]
        {
            audioProcessor.switchAB (slot);
            syncUIFromProcessor();
        };
        addAndMakeVisible (abButtons[i]);
    }

    // Top-right Settings / Preset File Menu Gear Icon
    settingsButton.setComponentID ("transparentHit");
    settingsButton.setTooltip ("Presets & Options");
    settingsButton.onClick = [this] { openSettingsMenu(); };
    addAndMakeVisible (settingsButton);

    setResizable (true, true);
    setResizeLimits (812, 310, 1624, 620);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (static_cast<double> (referenceWidth) / static_cast<double> (referenceHeight));
    setSize (1015, 388);

    enforceSoftwareRendererOnWindows();
    syncUIFromProcessor();
    startTimerHz (30);
}

DARoomAudioProcessorEditor::~DARoomAudioProcessorEditor()
{
    stopTimer();
    for (auto* s : primarySliders)
    {
        s->onValueChange = nullptr;
        s->setLookAndFeel (nullptr);
    }
    lowCutTrackSlider.onValueChange = nullptr;
    highCutTrackSlider.onValueChange = nullptr;
    lowCutTrackSlider.setLookAndFeel (nullptr);
    highCutTrackSlider.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void DARoomAudioProcessorEditor::parentHierarchyChanged()
{
    AudioProcessorEditor::parentHierarchyChanged();
    enforceSoftwareRendererOnWindows();
}

void DARoomAudioProcessorEditor::visibilityChanged()
{
    AudioProcessorEditor::visibilityChanged();
    enforceSoftwareRendererOnWindows();
}

void DARoomAudioProcessorEditor::enforceSoftwareRendererOnWindows()
{
   #if JUCE_WINDOWS
    // JUCE 8.0.6 defaults Windows peers to engine 1 (Direct2D / DirectComposition), which can
    // fail to render large bitmaps inside DAW child HWNDs or hybrid multi-GPU configurations.
    // Engine 0 is JUCE's rock-solid GDI DIBSection Software Renderer.
    if (auto* peer = getPeer())
    {
        if (peer->getCurrentRenderingEngine() != 0)
        {
            peer->setCurrentRenderingEngine (0);
            repaint();
        }
    }
   #endif
}

void DARoomAudioProcessorEditor::ensureScaledBackground()
{
    if (! background.isValid())
        return;

    const int targetW = getWidth();
    const int targetH = getHeight();
    if (targetW <= 0 || targetH <= 0)
        return;

    if (scaledBackground.isValid()
        && scaledBackground.getWidth() == targetW
        && scaledBackground.getHeight() == targetH)
    {
        return;
    }

    juce::Image scaled (juce::Image::ARGB, targetW, targetH, true, juce::SoftwareImageType{});
    {
        juce::Graphics sg (scaled);
        sg.fillAll (juce::Colour (0xff050c12));
        sg.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        sg.drawImage (background, juce::Rectangle<float> (0.0f, 0.0f,
                                                          static_cast<float> (targetW),
                                                          static_cast<float> (targetH)));
    }

    // Guarantee opaque alpha = 0xff across every resampled pixel
    {
        const juce::Image::BitmapData dst (scaled, juce::Image::BitmapData::readWrite);
        for (int y = 0; y < targetH; ++y)
        {
            auto* dLine = dst.getLinePointer (y);
            for (int x = 0; x < targetW; ++x)
            {
                auto* px = reinterpret_cast<juce::PixelARGB*> (dLine + x * dst.pixelStride);
                px->setARGB (0xff, px->getRed(), px->getGreen(), px->getBlue());
            }
        }
    }

    scaledBackground = scaled;
}

juce::Rectangle<int> DARoomAudioProcessorEditor::scaledBounds (float x, float y, float w, float h) const
{
    const float sx = static_cast<float> (getWidth()) / static_cast<float> (referenceWidth);
    const float sy = static_cast<float> (getHeight()) / static_cast<float> (referenceHeight);
    return {
        juce::roundToInt (x * sx),
        juce::roundToInt (y * sy),
        juce::roundToInt (w * sx),
        juce::roundToInt (h * sy)
    };
}

juce::Rectangle<float> DARoomAudioProcessorEditor::scaledFloatBounds (float x, float y, float w, float h) const
{
    const float sx = static_cast<float> (getWidth()) / static_cast<float> (referenceWidth);
    const float sy = static_cast<float> (getHeight()) / static_cast<float> (referenceHeight);
    return { x * sx, y * sy, w * sx, h * sy };
}

void DARoomAudioProcessorEditor::timerCallback()
{
    enforceSoftwareRendererOnWindows();
    syncUIFromProcessor();
}

void DARoomAudioProcessorEditor::syncUIFromProcessor()
{
    presetTitleButton.setButtonText (audioProcessor.getCurrentPresetDisplayName());

    const int liveMode = audioProcessor.getMode();
    if (liveMode != cachedMode)
    {
        cachedMode = liveMode;
        updateModeButtons (liveMode);
        repaint();
    }

    const int liveAB = audioProcessor.getActiveAB();
    if (liveAB != cachedAB)
    {
        cachedAB = liveAB;
        updateABButtons (liveAB);
    }

    updateValueLabels();
}

void DARoomAudioProcessorEditor::updateModeButtons (int modeIndex)
{
    for (size_t i = 0; i < modeButtons.size(); ++i)
        modeButtons[i].setToggleState (static_cast<int> (i) == modeIndex, juce::dontSendNotification);
}

void DARoomAudioProcessorEditor::updateABButtons (int abSlot)
{
    abButtons[0].setToggleState (abSlot == 0, juce::dontSendNotification);
    abButtons[1].setToggleState (abSlot == 1, juce::dontSendNotification);
}

void DARoomAudioProcessorEditor::stepPreset (int delta)
{
    int idx = audioProcessor.getSelectedPresetIndex() + delta;
    if (idx < 0)
        idx = DARoomPresets::numFactoryPresets - 1;
    else if (idx >= DARoomPresets::numFactoryPresets)
        idx = 0;

    audioProcessor.applyFactoryPreset (idx);
    syncUIFromProcessor();
}

void DARoomAudioProcessorEditor::openPresetMenu()
{
    juce::PopupMenu menu;
    const int currentIdx = audioProcessor.getSelectedPresetIndex();

    for (int cat = 0; cat < 5; ++cat)
    {
        juce::PopupMenu sub;
        for (int i = 0; i < DARoomPresets::numFactoryPresets; ++i)
        {
            if (DARoomPresets::factoryPresets[i].mode == cat)
                sub.addItem (i + 1, DARoomPresets::factoryPresets[i].name, true, i == currentIdx);
        }
        menu.addSubMenu (juce::String (DARoomPresets::modeDisplayNames[cat]) + " PRESETS", sub, true, nullptr,
                         DARoomPresets::factoryPresets[currentIdx].mode == cat);
    }

    menu.addSeparator();
    menu.addItem (1001, "Load Preset File (.drpreset)...");
    menu.addItem (1002, "Save Preset File (.drpreset)...");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetTitleButton),
                        [safeThis = juce::Component::SafePointer<DARoomAudioProcessorEditor> (this)] (int result)
                        {
                            if (safeThis == nullptr || result == 0)
                                return;

                            if (result >= 1 && result <= DARoomPresets::numFactoryPresets)
                            {
                                safeThis->audioProcessor.applyFactoryPreset (result - 1);
                                safeThis->syncUIFromProcessor();
                            }
                            else if (result == 1001)
                            {
                                safeThis->promptLoadPreset();
                            }
                            else if (result == 1002)
                            {
                                safeThis->promptSavePreset();
                            }
                        });
}

void DARoomAudioProcessorEditor::openSettingsMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("D.A ROOM OPTIONS");
    menu.addItem (1, "Load Preset (.drpreset)...");
    menu.addItem (2, "Save Preset (.drpreset)...");
    menu.addSeparator();
    menu.addItem (3, audioProcessor.getActiveAB() == 0 ? "Copy A to B" : "Copy B to A");
    menu.addItem (4, "Reset to Default (Deep Chamber)");
    menu.addSeparator();

    juce::PopupMenu zoomMenu;
    zoomMenu.addItem (10, "75% (812 x 310)");
    zoomMenu.addItem (11, "100% (1015 x 388)");
    zoomMenu.addItem (12, "125% (1269 x 484)");
    zoomMenu.addItem (13, "150% (1522 x 581)");
    menu.addSubMenu ("Window Size", zoomMenu);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&settingsButton),
                        [safeThis = juce::Component::SafePointer<DARoomAudioProcessorEditor> (this)] (int result)
                        {
                            if (safeThis == nullptr || result == 0)
                                return;

                            switch (result)
                            {
                                case 1: safeThis->promptLoadPreset(); break;
                                case 2: safeThis->promptSavePreset(); break;
                                case 3: safeThis->audioProcessor.copyActiveABToOther(); break;
                                case 4:
                                    safeThis->audioProcessor.applyFactoryPreset (DARoomPresets::defaultPresetIndex);
                                    safeThis->syncUIFromProcessor();
                                    break;
                                case 10: safeThis->setSize (812, 310); break;
                                case 11: safeThis->setSize (1015, 388); break;
                                case 12: safeThis->setSize (1269, 484); break;
                                case 13: safeThis->setSize (1522, 581); break;
                                default: break;
                            }
                        });
}

void DARoomAudioProcessorEditor::promptLoadPreset()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Load D.A Room Preset",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
        "*.drpreset");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safeThis = juce::Component::SafePointer<DARoomAudioProcessorEditor> (this)] (const juce::FileChooser& fc)
                              {
                                  if (safeThis == nullptr)
                                      return;
                                  const auto chosen = fc.getResult();
                                  if (chosen.existsAsFile() && safeThis->audioProcessor.loadPresetFromFile (chosen))
                                      safeThis->syncUIFromProcessor();
                              });
}

void DARoomAudioProcessorEditor::promptSavePreset()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Save D.A Room Preset",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
            .getChildFile (audioProcessor.getCurrentPresetDisplayName() + ".drpreset"),
        "*.drpreset");

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                              [safeThis = juce::Component::SafePointer<DARoomAudioProcessorEditor> (this)] (const juce::FileChooser& fc)
                              {
                                  if (safeThis == nullptr)
                                      return;
                                  auto chosen = fc.getResult();
                                  if (chosen != juce::File())
                                  {
                                      if (! chosen.hasFileExtension (".drpreset"))
                                          chosen = chosen.withFileExtension (".drpreset");
                                      if (safeThis->audioProcessor.savePresetToFile (chosen))
                                          safeThis->syncUIFromProcessor();
                                  }
                              });
}

void DARoomAudioProcessorEditor::updateValueLabels()
{
    const auto val = [this] (size_t i) { return primarySliders[i]->getValue(); };

    values[0].setText (juce::String (val (0), 0) + "%", juce::dontSendNotification);
    values[1].setText (juce::String (val (1), 0) + " ms", juce::dontSendNotification);
    values[2].setText (juce::String (val (2), 2) + " s", juce::dontSendNotification);
    values[3].setText (juce::String (val (3), 0) + "%", juce::dontSendNotification);
    values[4].setText (juce::String (val (4), 0) + "%", juce::dontSendNotification);
    values[5].setText (juce::String (val (5), 0) + "%", juce::dontSendNotification);
    values[6].setText (juce::String (val (6), 0) + "%", juce::dontSendNotification);
    values[7].setText (juce::String (val (7), 0) + "%", juce::dontSendNotification);

    const double lc = val (8);
    values[8].setText (lc >= 1000.0 ? juce::String (lc / 1000.0, 2) + " kHz"
                                    : juce::String (lc, 0) + " Hz",
                       juce::dontSendNotification);
    values[9].setText (juce::String (val (9) / 1000.0, 1) + " kHz", juce::dontSendNotification);
}

void DARoomAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff050c12));
    ensureScaledBackground();

    if (scaledBackground.isValid())
    {
        g.setOpacity (1.0f);
        g.drawImageAt (scaledBackground, 0, 0);
    }
    else if (background.isValid())
    {
        g.setOpacity (1.0f);
        g.drawImage (background, getLocalBounds().toFloat());
    }

    // Dynamic active cyan glow bar under the active reverb mode
    const int activeMode = juce::jlimit (0, 4, audioProcessor.getMode());
    const float slotW = 919.0f / 5.0f;
    const float glowX = 556.0f + static_cast<float> (activeMode) * slotW + slotW * 0.16f;
    const float glowW = slotW * 0.68f;
    const auto glowBar = scaledFloatBounds (glowX, 533.5f, glowW, 3.2f);
    g.setColour (juce::Colour (0x445cd9ff));
    g.fillRoundedRectangle (glowBar.expanded (4.0f, 2.5f), 3.0f);
    g.setColour (juce::Colour (0xff90ebff));
    g.fillRoundedRectangle (glowBar, 1.8f);
}

void DARoomAudioProcessorEditor::resized()
{
    ensureScaledBackground();

    static constexpr float knobRefs[10][4] = {
        {   63.0f, 222.0f, 166.0f, 166.0f }, // MIX
        {  301.0f, 222.0f, 166.0f, 166.0f }, // PREDELAY
        { 1567.0f, 222.0f, 166.0f, 166.0f }, // DECAY
        { 1799.0f, 222.0f, 166.0f, 166.0f }, // SIZE
        {   67.0f, 560.0f, 122.0f, 122.0f }, // DIFFUSION
        {  381.0f, 560.0f, 122.0f, 122.0f }, // MODULATION
        {  679.0f, 560.0f, 122.0f, 122.0f }, // DAMPING
        {  978.0f, 560.0f, 122.0f, 122.0f }, // WIDTH
        { 1280.0f, 575.0f,  92.0f,  92.0f }, // LOW CUT KNOB
        { 1634.0f, 575.0f,  92.0f,  92.0f }  // HIGH CUT KNOB
    };

    for (size_t i = 0; i < 10; ++i)
        primarySliders[i]->setBounds (scaledBounds (knobRefs[i][0], knobRefs[i][1], knobRefs[i][2], knobRefs[i][3]));

    lowCutTrackSlider.setBounds  (scaledBounds (1382.0f, 678.0f, 192.0f, 28.0f));
    highCutTrackSlider.setBounds (scaledBounds (1744.0f, 678.0f, 208.0f, 28.0f));

    static constexpr float valueRefs[10][4] = {
        {   91.0f, 437.0f, 110.0f, 30.0f },
        {  324.0f, 437.0f, 120.0f, 30.0f },
        { 1590.0f, 437.0f, 120.0f, 30.0f },
        { 1827.0f, 437.0f, 110.0f, 30.0f },
        {  195.0f, 679.0f, 110.0f, 26.0f },
        {  495.0f, 679.0f, 110.0f, 26.0f },
        {  800.0f, 679.0f, 110.0f, 26.0f },
        { 1095.0f, 679.0f, 110.0f, 26.0f },
        { 1397.0f, 637.0f, 110.0f, 28.0f },
        { 1760.0f, 637.0f, 116.0f, 28.0f }
    };

    const float sy = static_cast<float> (getHeight()) / static_cast<float> (referenceHeight);
    const float topValFont = juce::jmax (10.0f, 24.5f * sy);
    const float botValFont = juce::jmax (8.5f, 19.5f * sy);

    for (size_t i = 0; i < 10; ++i)
    {
        values[i].setBounds (scaledBounds (valueRefs[i][0], valueRefs[i][1], valueRefs[i][2], valueRefs[i][3]));
        values[i].setFont (juce::Font (juce::FontOptions (i < 4 ? topValFont : botValFont)));
    }

    prevPresetButton.setBounds  (scaledBounds ( 714.0f, 74.0f,  80.0f, 58.0f));
    presetTitleButton.setBounds (scaledBounds ( 794.0f, 74.0f, 442.0f, 58.0f));
    nextPresetButton.setBounds  (scaledBounds (1236.0f, 74.0f,  80.0f, 58.0f));

    const float slotW = 919.0f / 5.0f;
    for (size_t i = 0; i < 5; ++i)
        modeButtons[i].setBounds (scaledBounds (556.0f + static_cast<float> (i) * slotW + 3.0f, 466.0f, slotW - 6.0f, 68.0f));

    abButtons[0].setBounds   (scaledBounds (1741.0f, 74.0f, 78.0f, 59.0f));
    abButtons[1].setBounds   (scaledBounds (1819.0f, 74.0f, 78.0f, 59.0f));
    settingsButton.setBounds (scaledBounds (1924.0f, 74.0f, 58.0f, 58.0f));
}
