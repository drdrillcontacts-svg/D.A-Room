#include "PluginEditor.h"
#include "BinaryData.h"
#include <cmath>

namespace
{
struct FactoryPreset
{
    const char* name;
    float mix, predelay, decay, size;
    int mode;
    float diffusion, modulation, damping, width, lowCut, highCut;
};

const FactoryPreset factoryPresets[] = {
    { "Studio Tight", 12f, 5f, 0.45f, 28f, 0, 65f, 8f, 62f, 55f, 120f, 15000f },
    { "Vocal Booth", 14f, 8f, 0.55f, 32f, 0, 70f, 12f, 58f, 65f, 150f, 14500f },
    { "Small Studio", 18f, 10f, 0.7f, 38f, 0, 72f, 15f, 55f, 72f, 100f, 16000f },
    { "Wood Room", 20f, 12f, 0.9f, 42f, 0, 78f, 18f, 60f, 68f, 90f, 14000f },
    { "Drum Room", 22f, 4f, 0.85f, 52f, 0, 82f, 22f, 48f, 80f, 70f, 17000f },
    { "Bright Room", 18f, 7f, 1.1f, 48f, 0, 75f, 26f, 32f, 78f, 130f, 18000f },
    { "Living Room", 20f, 14f, 1.2f, 50f, 0, 80f, 10f, 55f, 72f, 100f, 13500f },
    { "Tracking Room", 24f, 16f, 1.45f, 58f, 0, 84f, 18f, 50f, 76f, 80f, 16000f },
    { "Wide Room", 25f, 12f, 1.35f, 62f, 0, 88f, 30f, 45f, 100f, 90f, 16500f },
    { "Vintage Room", 22f, 18f, 1.6f, 55f, 0, 76f, 6f, 72f, 65f, 100f, 9500f },
    { "Small Hall", 20f, 18f, 1.8f, 55f, 1, 72f, 12f, 48f, 72f, 100f, 15000f },
    { "Concert Hall", 25f, 24f, 3.2f, 75f, 1, 84f, 18f, 42f, 88f, 80f, 15500f },
    { "Dark Hall", 22f, 20f, 4.2f, 78f, 1, 82f, 12f, 72f, 85f, 120f, 8000f },
    { "Bright Hall", 24f, 18f, 2.8f, 68f, 1, 80f, 22f, 30f, 90f, 100f, 18000f },
    { "Cathedral", 24f, 35f, 7.5f, 92f, 1, 90f, 16f, 68f, 92f, 90f, 10000f },
    { "Arena", 28f, 32f, 5.8f, 95f, 1, 86f, 28f, 48f, 100f, 100f, 14000f },
    { "Orchestral Depth", 22f, 26f, 4.6f, 84f, 1, 88f, 20f, 52f, 92f, 70f, 14500f },
    { "Piano Hall", 18f, 22f, 3.1f, 70f, 1, 82f, 10f, 38f, 82f, 80f, 16000f },
    { "Grand Hall", 27f, 28f, 5.2f, 88f, 1, 86f, 24f, 46f, 96f, 90f, 15000f },
    { "Soft Hall", 20f, 25f, 3.8f, 76f, 1, 90f, 8f, 70f, 80f, 110f, 10500f },
    { "Vocal Plate", 22f, 18f, 1.8f, 58f, 2, 78f, 25f, 32f, 82f, 120f, 16000f },
    { "Vintage Plate", 25f, 12f, 2.2f, 62f, 2, 70f, 10f, 65f, 75f, 100f, 11000f },
    { "Bright Plate", 22f, 8f, 2.6f, 65f, 2, 76f, 30f, 25f, 82f, 150f, 18000f },
    { "Drum Plate", 18f, 4f, 1.35f, 50f, 2, 80f, 18f, 40f, 78f, 70f, 17000f },
    { "Snare Bloom", 20f, 6f, 1.7f, 54f, 2, 82f, 24f, 38f, 84f, 80f, 16000f },
    { "Smooth Plate", 24f, 16f, 2.4f, 60f, 2, 88f, 12f, 58f, 86f, 130f, 12500f },
    { "Guitar Plate", 18f, 10f, 1.6f, 48f, 2, 75f, 20f, 45f, 74f, 90f, 14500f },
    { "Pop Plate", 20f, 14f, 2.1f, 57f, 2, 80f, 28f, 30f, 82f, 120f, 17000f },
    { "Dark Plate", 24f, 20f, 2.8f, 68f, 2, 82f, 10f, 75f, 88f, 120f, 8500f },
    { "Classic Plate", 23f, 11f, 2.0f, 60f, 2, 72f, 16f, 52f, 78f, 100f, 14000f },
    { "Deep Chamber", 26f, 25f, 4.5f, 78f, 3, 82f, 18f, 50f, 78f, 100f, 14000f },
    { "Stone Chamber", 24f, 20f, 3.6f, 72f, 3, 78f, 14f, 60f, 80f, 120f, 11500f },
    { "Warm Chamber", 28f, 20f, 2.4f, 68f, 3, 80f, 12f, 68f, 76f, 100f, 12000f },
    { "Dark Chamber", 20f, 16f, 3.2f, 64f, 3, 84f, 8f, 78f, 82f, 140f, 9000f },
    { "Gold Chamber", 26f, 14f, 2.8f, 70f, 3, 86f, 22f, 42f, 88f, 90f, 15000f },
    { "Vocal Chamber", 24f, 22f, 2.7f, 66f, 3, 80f, 20f, 48f, 82f, 130f, 15000f },
    { "Cinematic Chamber", 25f, 28f, 5.6f, 88f, 3, 88f, 26f, 50f, 96f, 80f, 13500f },
    { "Tight Chamber", 16f, 10f, 1.5f, 45f, 3, 74f, 12f, 58f, 68f, 100f, 14000f },
    { "Velvet Chamber", 24f, 18f, 3.4f, 74f, 3, 90f, 14f, 72f, 86f, 120f, 11000f },
    { "Wide Chamber", 28f, 24f, 4.2f, 82f, 3, 88f, 32f, 45f, 100f, 100f, 14500f },
    { "Ambient Cloud", 30f, 35f, 8.5f, 95f, 4, 88f, 22f, 65f, 96f, 100f, 12000f },
    { "Dream Space", 32f, 30f, 7.2f, 90f, 4, 92f, 28f, 60f, 100f, 100f, 14000f },
    { "Cosmic Drift", 28f, 42f, 10.0f, 100f, 4, 92f, 36f, 52f, 100f, 80f, 13000f },
    { "Frozen Air", 22f, 28f, 5.8f, 86f, 4, 94f, 20f, 72f, 90f, 180f, 10500f },
    { "Soft Focus", 18f, 24f, 4.2f, 78f, 4, 90f, 10f, 76f, 86f, 130f, 10000f },
    { "Wide Synth", 30f, 18f, 5.2f, 88f, 4, 88f, 32f, 48f, 100f, 100f, 15000f },
    { "Endless Horizon", 30f, 50f, 12.0f, 100f, 4, 94f, 26f, 68f, 100f, 70f, 11500f },
    { "Airy Vocal", 24f, 28f, 5.0f, 84f, 4, 92f, 24f, 42f, 90f, 150f, 16000f },
    { "Shimmering Tail", 26f, 22f, 6.4f, 92f, 4, 90f, 38f, 28f, 96f, 120f, 18000f },
    { "Afterglow", 25f, 32f, 7.8f, 94f, 4, 96f, 18f, 62f, 98f, 90f, 12500f },
};
constexpr int numFactoryPresets = (int) (sizeof(factoryPresets) / sizeof(factoryPresets[0]));
const char* modeNames[] = { "ROOM", "HALL", "PLATE", "CHAMBER", "AMBIENCE" };
const char* parameterIDs[] = { "mix", "predelay", "decay", "size", "diffusion",
                               "modulation", "damping", "width", "lowcut", "highcut" };
}

void DARoomLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                          float pos, float start, float end, juce::Slider&)
{
    const float diameter = (float) juce::jmin(w, h) - 5.0f;
    const float cx = (float)x + (float)w * 0.5f;
    const float cy = (float)y + (float)h * 0.5f;
    const float r = diameter * 0.5f;
    const auto bounds = juce::Rectangle<float>(cx-r, cy-r, diameter, diameter);

    g.setColour(juce::Colour(0x90000000));
    g.fillEllipse(bounds.expanded(2.0f));
    juce::ColourGradient face(juce::Colour(0xff18252c), cx, cy-r,
                              juce::Colour(0xff03080c), cx, cy+r, false);
    g.setGradientFill(face);
    g.fillEllipse(bounds);
    g.setColour(juce::Colour(0xff29424e));
    g.drawEllipse(bounds, 1.0f);

    const float angle = start + pos * (end-start);
    juce::Path track;
    track.addCentredArc(cx, cy, r-3.0f, r-3.0f, 0.0f, start, end, true);
    g.setColour(juce::Colour(0xff29414b));
    g.strokePath(track, juce::PathStrokeType(2.0f));
    juce::Path arc;
    arc.addCentredArc(cx, cy, r-3.0f, r-3.0f, 0.0f, start, angle, true);
    g.setColour(juce::Colour(0xff79dfff));
    g.strokePath(arc, juce::PathStrokeType(2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Point<float> p(cx + std::sin(angle) * (r*0.63f),
                         cy - std::cos(angle) * (r*0.63f));
    g.setColour(juce::Colour(0xffdff7ff));
    g.drawLine(cx, cy, p.x, p.y, 2.2f);
    g.setColour(juce::Colour(0xffdff7ff));
    g.fillEllipse(cx-2.0f, cy-2.0f, 4.0f, 4.0f);
}

DARoomAudioProcessorEditor::DARoomAudioProcessorEditor (DARoomAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    background = juce::ImageFileFormat::loadFrom(BinaryData::DAroom_GUI_png, BinaryData::DAroom_GUI_pngSize);
    setSize (1015, 388);
    setResizable (true, true);
    setResizeLimits (810, 310, 1624, 620);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) referenceWidth / (double) referenceHeight);

    const juce::StringArray names { "Mix", "Predelay", "Decay", "Size", "Diffusion",
                                    "Modulation", "Damping", "Width", "Low Cut", "High Cut" };
    for (int i=0; i<(int)sliders.size(); ++i)
    {
        auto* s = sliders[(size_t)i];
        s->setLookAndFeel(&customLookAndFeel);
        if (i < 8)
        {
            s->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            s->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        }
        else
        {
            s->setSliderStyle(juce::Slider::LinearHorizontal);
            s->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            s->setColour(juce::Slider::trackColourId, juce::Colour(0xff64c9f3));
            s->setColour(juce::Slider::thumbColourId, juce::Colour(0xffdff7ff));
        }
        s->setTooltip(names[i]);
        addAndMakeVisible(*s);
        values[(size_t)i].setJustificationType(juce::Justification::centred);
        values[(size_t)i].setFont(juce::Font(9.5f));
        values[(size_t)i].setColour(juce::Label::textColourId, juce::Colour(0xffa9d9ee));
        values[(size_t)i].setInterceptsMouseClicks(false, false);
        addAndMakeVisible(values[(size_t)i]);
        attachments[(size_t)i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            audioProcessor.parameters, parameterIDs[i], *s);
    }

    // Preset browser: popup sections are categorized, while reverb modes below are direct-click buttons.
    presetSelector.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xee061018));
    presetSelector.setColour(juce::ComboBox::textColourId, juce::Colour(0xffe3f5ff));
    presetSelector.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff27566a));
    int itemID = 1;
    for (int category=0; category<5; ++category)
    {
        presetSelector.addSectionHeading(juce::String(modeNames[category]) + " PRESETS");
        for (int i=0; i<numFactoryPresets; ++i)
            if (factoryPresets[i].mode == category)
                presetSelector.addItem(factoryPresets[i].name, itemID++);
    }
    presetSelector.setSelectedId(31, juce::dontSendNotification); // Deep Chamber
    presetSelector.onChange = [this]
    {
        const int id = presetSelector.getSelectedId();
        if (id < 1 || id > numFactoryPresets) return;
        const auto& pr = factoryPresets[id-1];
        audioProcessor.applyPreset(pr.mix, pr.predelay, pr.decay, pr.size, pr.mode,
                                   pr.diffusion, pr.modulation, pr.damping, pr.width, pr.lowCut, pr.highCut);
        activeMode = pr.mode;
        updateModeButtons();
        updateValueLabels();
    };
    addAndMakeVisible(presetSelector);

    for (int i=0; i<5; ++i)
    {
        modeButtons[(size_t)i].setButtonText(modeNames[i]);
        modeButtons[(size_t)i].setClickingTogglesState(false);
        modeButtons[(size_t)i].setColour(juce::TextButton::buttonColourId, juce::Colour(0x88061118));
        modeButtons[(size_t)i].setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff0b3444));
        modeButtons[(size_t)i].setColour(juce::TextButton::textColourOffId, juce::Colour(0xffa8c7d5));
        modeButtons[(size_t)i].setColour(juce::TextButton::textColourOnId, juce::Colour(0xff85e3ff));
        modeButtons[(size_t)i].onClick = [this, i]
        {
            activeMode = i;
            audioProcessor.setMode(i);
            updateModeButtons();
        };
        addAndMakeVisible(modeButtons[(size_t)i]);
    }

    abButtons[0].setButtonText("A");
    abButtons[1].setButtonText("B");
    for (auto& b : abButtons)
    {
        b.setColour(juce::TextButton::buttonColourId, juce::Colour(0x88061118));
        b.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffd5eaf2));
        addAndMakeVisible(b);
    }
    captureSnapshot(snapshotA);
    snapshotB = snapshotA;
    hasSnapshotA = hasSnapshotB = true;
    abButtons[0].onClick = [this]
    {
        if (activeAB == 1) captureSnapshot(snapshotB);
        if (hasSnapshotA) restoreSnapshot(snapshotA);
        activeAB = 0;
    };
    abButtons[1].onClick = [this]
    {
        if (activeAB == 0) captureSnapshot(snapshotA);
        if (!hasSnapshotB) { snapshotB = snapshotA; hasSnapshotB = true; }
        if (hasSnapshotB) restoreSnapshot(snapshotB);
        activeAB = 1;
    };
    updateModeButtons();
    updateValueLabels();
}

DARoomAudioProcessorEditor::~DARoomAudioProcessorEditor()
{
    for (auto* s : sliders) s->setLookAndFeel(nullptr);
}

juce::Rectangle<int> DARoomAudioProcessorEditor::scaledBounds(float x,float y,float w,float h) const
{
    const float sx=(float)getWidth()/(float)referenceWidth;
    const float sy=(float)getHeight()/(float)referenceHeight;
    return { juce::roundToInt(x*sx), juce::roundToInt(y*sy),
             juce::roundToInt(w*sx), juce::roundToInt(h*sy) };
}


void DARoomAudioProcessorEditor::captureSnapshot (std::array<float, 11>& state)
{
    const char* ids[] = { "mix", "predelay", "decay", "size", "mode", "diffusion",
                          "modulation", "damping", "width", "lowcut", "highcut" };
    for (int i = 0; i < 11; ++i)
        if (auto* value = audioProcessor.parameters.getRawParameterValue(ids[i]))
            state[(size_t)i] = value->load();
}

void DARoomAudioProcessorEditor::restoreSnapshot (const std::array<float, 11>& state)
{
    const char* ids[] = { "mix", "predelay", "decay", "size", "mode", "diffusion",
                          "modulation", "damping", "width", "lowcut", "highcut" };
    for (int i = 0; i < 11; ++i)
        if (auto* param = audioProcessor.parameters.getParameter(ids[i]))
            param->setValueNotifyingHost(param->convertTo0to1(state[(size_t)i]));
    activeMode = juce::jlimit(0, 4, (int) state[4]);
    updateModeButtons();
    updateValueLabels();
}

void DARoomAudioProcessorEditor::updateModeButtons()
{
    for (int i=0;i<5;++i)
    {
        modeButtons[(size_t)i].setToggleState(i==activeMode, juce::dontSendNotification);
        modeButtons[(size_t)i].setColour(juce::TextButton::buttonColourId,
                                         i==activeMode ? juce::Colour(0xdd0b3545) : juce::Colour(0x99061118));
        modeButtons[(size_t)i].setColour(juce::TextButton::textColourOffId,
                                         i==activeMode ? juce::Colour(0xff8be7ff) : juce::Colour(0xffa8c7d5));
    }
}

void DARoomAudioProcessorEditor::updateValueLabels()
{
    const auto val = [this](int i) { return sliders[(size_t)i]->getValue(); };
    values[0].setText(juce::String(val(0),0)+"%",juce::dontSendNotification);
    values[1].setText(juce::String(val(1),0)+" ms",juce::dontSendNotification);
    values[2].setText(juce::String(val(2),2)+" s",juce::dontSendNotification);
    values[3].setText(juce::String(val(3),0)+"%",juce::dontSendNotification);
    values[4].setText(juce::String(val(4),0)+"%",juce::dontSendNotification);
    values[5].setText(juce::String(val(5),0)+"%",juce::dontSendNotification);
    values[6].setText(juce::String(val(6),0)+"%",juce::dontSendNotification);
    values[7].setText(juce::String(val(7),0)+"%",juce::dontSendNotification);
    values[8].setText(val(8)>=1000.0?juce::String(val(8)/1000.0,2)+" kHz":juce::String(val(8),0)+" Hz",juce::dontSendNotification);
    values[9].setText(juce::String(val(9)/1000.0,1)+" kHz",juce::dontSendNotification);
}

void DARoomAudioProcessorEditor::paint(juce::Graphics& g)
{
    if (background.isValid())
        g.drawImage(background, getLocalBounds().toFloat());
    else
    {
        g.fillAll(juce::Colour(0xff061018));
        g.setColour(juce::Colour(0xffb5eaff));
        g.setFont(juce::Font(22.0f));
        g.drawText("D.A Room", 20, 8, 240, 36, juce::Justification::left);
    }
    // Cover baked-in values so the displayed readouts stay in sync with live parameters.
    g.setColour(juce::Colour(0xe9061018));
    for (auto r : { scaledBounds(95,426,105,42), scaledBounds(315,426,115,42),
                    scaledBounds(1570,426,120,42), scaledBounds(1790,426,110,42),
                    scaledBounds(145,672,115,30), scaledBounds(390,672,115,30),
                    scaledBounds(690,672,115,30), scaledBounds(990,672,115,30),
                    scaledBounds(1360,638,145,38), scaledBounds(1725,638,150,38) })
        g.fillRoundedRectangle(r.toFloat(), 3.0f);
    // Add a subtle glass panel under the five direct-click mode buttons.
    g.setColour(juce::Colour(0xd9061018));
    g.fillRoundedRectangle(scaledBounds(550,468,910,67).toFloat(), 12.0f);
    g.setColour(juce::Colour(0x773b9bb8));
    g.drawRoundedRectangle(scaledBounds(550,468,910,67).toFloat(), 12.0f, 1.0f);
}

void DARoomAudioProcessorEditor::resized()
{
    const std::array<juce::Rectangle<int>,10> refs {{
        {72,225,145,145}, {307,225,145,145}, {1562,225,145,145}, {1786,225,145,145},
        {70,566,115,115}, {374,566,115,115}, {676,566,115,115}, {972,566,115,115},
        {1358,650,215,42}, {1717,650,220,42}
    }};
    for (int i=0;i<10;++i)
    {
        const auto& r=refs[(size_t)i];
        sliders[(size_t)i]->setBounds(scaledBounds((float)r.getX(),(float)r.getY(),(float)r.getWidth(),(float)r.getHeight()));
    }

    const std::array<juce::Rectangle<int>,10> valueRefs {{
        {95,425,105,22},{315,425,115,22},{1570,425,120,22},{1790,425,110,22},
        {145,672,115,20},{390,672,115,20},{690,672,115,20},{990,672,115,20},
        {1360,638,145,24},{1725,638,150,24}
    }};
    for(int i=0;i<10;++i) values[(size_t)i].setBounds(scaledBounds((float)valueRefs[(size_t)i].getX(),
        (float)valueRefs[(size_t)i].getY(),(float)valueRefs[(size_t)i].getWidth(),(float)valueRefs[(size_t)i].getHeight()));

    presetSelector.setBounds(scaledBounds(708,74,598,58));
    const std::array<juce::Rectangle<int>,5> modeRefs {{
        {560,474,175,53},{740,474,175,53},{920,474,175,53},{1100,474,175,53},{1280,474,175,53}
    }};
    for(int i=0;i<5;++i) modeButtons[(size_t)i].setBounds(scaledBounds(
        (float)modeRefs[(size_t)i].getX(),(float)modeRefs[(size_t)i].getY(),
        (float)modeRefs[(size_t)i].getWidth(),(float)modeRefs[(size_t)i].getHeight()));
    abButtons[0].setBounds(scaledBounds(1730,74,80,58));
    abButtons[1].setBounds(scaledBounds(1810,74,80,58));
    for(int i=0;i<10;++i)
        sliders[(size_t)i]->onValueChange = [this] { updateValueLabels(); };
}
