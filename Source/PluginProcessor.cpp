#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace
{
struct ModeAcousticProfile
{
    float baseMinDelayMs;    // FDN shortest line at Size=100%
    float decayMultiplier;   // Scales Decay parameter into mode-authentic Midband RT60
    float lfRt60Ratio;       // Low-frequency (<200 Hz) RT60 multiplier vs Midband
    float hfRt60Ratio;       // High-frequency (>2.8 kHz) natural RT60 multiplier vs Midband
    float inputHpHz;         // Pre-tank highpass voicing
    float inputLpHz;         // Pre-tank lowpass voicing
    float erGain;            // Early reflections level (unity-power calibrated)
    float lateGain;          // Late FDN tank level (unity-power calibrated)
    float lateOffsetMs;      // Pre-bloom gap between ERs and Late FDN onset
    float diffTimeScale;     // Input allpass delay time multiplier
    float diffMinFeedback;   // Minimum allpass density floor for mode character
    float modRateHz;         // Base LFO rate (Hz)
    float modDepthScale;     // Tank modulation excursion multiplier
    float stereoSpreadBias;  // Natural mode stereo width multiplier
    float shimmerSend;       // Octave-up (+12st) crystalline halo injected into FDN tank
    float erTapMsL[6];       // Mode-specific stereo Early Reflection geometry (ms)
    float erTapMsR[6];
};

// 5 distinct studio reverb topologies: ROOM, HALL, PLATE, CHAMBER, AMBIENCE
constexpr ModeAcousticProfile kModeProfiles[5] = {
    // 0: ROOM — Tight, punchy studio wood room with strong 3D wall reflections
    {
        11.8f, 0.38f, 0.65f, 0.50f,  70.0f, 14000.0f,
        1.12f, 3.25f,  4.5f, 0.50f,  0.28f,  0.45f, 0.24f, 0.86f, 0.00f,
        {  2.3f,  5.1f,  8.7f, 12.9f, 17.8f, 23.9f },
        {  2.9f,  5.9f,  9.6f, 14.2f, 19.5f, 26.1f }
    },
    // 1: HALL — Grand Lexicon-style symphonic concert hall with 26ms pre-bloom & warm lows
    {
        36.0f, 1.15f, 1.28f, 0.44f,  38.0f, 11200.0f,
        0.58f, 3.95f, 26.0f, 1.10f,  0.52f,  0.55f, 1.08f, 1.10f, 0.00f,
        { 11.7f, 20.3f, 30.9f, 43.1f, 56.7f, 71.3f },
        { 13.9f, 23.7f, 35.1f, 47.9f, 61.9f, 77.1f }
    },
    // 2: PLATE — Instant-onset EMT-140 dense vocal/snare plate with crisp highs & tight lows
    {
        17.0f, 0.85f, 0.42f, 0.95f, 145.0f, 19000.0f,
        0.14f, 3.88f,  0.0f, 0.30f,  0.70f,  1.30f, 0.42f, 1.04f, 0.00f,
        {  1.4f,  2.9f,  4.7f,  6.8f,  9.3f, 12.1f },
        {  1.7f,  3.3f,  5.2f,  7.4f, 10.1f, 13.2f }
    },
    // 3: CHAMBER — Warm, creamy Capitol/Abbey Road plaster echo chamber for vocal/mix glue
    {
        24.0f, 0.88f, 0.92f, 0.25f,  95.0f,  7400.0f,
        0.98f, 4.95f, 11.5f, 0.78f,  0.56f,  0.70f, 0.65f, 0.96f, 0.00f,
        {  4.6f,  8.3f, 13.1f, 18.9f, 25.7f, 33.4f },
        {  5.3f,  9.4f, 14.6f, 20.8f, 27.9f, 36.1f }
    },
    // 4: AMBIENCE — Vast floating cloud space with slow bloom, deep drift & +12st shimmer air
    {
        46.0f, 1.55f, 1.15f, 0.76f,  60.0f, 16000.0f,
        0.38f, 3.45f, 36.0f, 1.40f,  0.68f,  0.36f, 1.55f, 1.20f, 0.28f,
        { 17.3f, 29.1f, 42.7f, 58.3f, 75.1f, 93.7f },
        { 19.9f, 32.9f, 47.3f, 63.9f, 81.7f, 99.8f }
    }
};

// Mutually coprime exponential delay ratios across the 8 FDN lines (1.00 .. 2.52)
constexpr float kFDNRatios[8] = {
    1.0000f, 1.1487f, 1.3195f, 1.5157f,
    1.7411f, 1.9999f, 2.2974f, 2.6390f
};

// In-loop Schroeder allpass base times (ms) for modal smearing inside each FDN line
constexpr float kTankApMs[8] = {
    1.63f, 2.11f, 2.69f, 3.17f,
    3.73f, 4.31f, 4.87f, 5.47f
};

// Early reflection distance attenuation & alternating acoustic polarity
constexpr float kErTapWeights[6] = {
    0.46f, -0.37f, 0.31f, -0.25f, 0.20f, -0.16f
};
} // namespace

DARoomAudioProcessor::DARoomAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "DARoomState", createParameterLayout())
{
    lowCutFilter.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    highCutFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

    captureSnapshot (snapshotA);
    snapshotB = snapshotA;
}

juce::AudioProcessorValueTreeState::ParameterLayout DARoomAudioProcessor::createParameterLayout()
{
    using FloatParam = juce::AudioParameterFloat;
    juce::AudioProcessorValueTreeState::ParameterLayout p;

    p.add (std::make_unique<FloatParam> (
        juce::ParameterID { "mix", 1 }, "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 26.0f, "%"));

    p.add (std::make_unique<FloatParam> (
        juce::ParameterID { "predelay", 1 }, "Predelay",
        juce::NormalisableRange<float> (0.0f, 250.0f, 0.1f, 0.55f), 25.0f, " ms"));

    p.add (std::make_unique<FloatParam> (
        juce::ParameterID { "decay", 1 }, "Decay",
        juce::NormalisableRange<float> (0.2f, 12.0f, 0.01f, 0.50f), 4.5f, " s"));

    p.add (std::make_unique<FloatParam> (
        juce::ParameterID { "size", 1 }, "Size",
        juce::NormalisableRange<float> (10.0f, 100.0f, 0.1f), 78.0f, "%"));

    p.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "mode", 1 }, "Reverb Mode",
        juce::StringArray { "Room", "Hall", "Plate", "Chamber", "Ambience" }, 3));

    p.add (std::make_unique<FloatParam> (
        juce::ParameterID { "diffusion", 1 }, "Diffusion",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 82.0f, "%"));

    p.add (std::make_unique<FloatParam> (
        juce::ParameterID { "modulation", 1 }, "Modulation",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 18.0f, "%"));

    p.add (std::make_unique<FloatParam> (
        juce::ParameterID { "damping", 1 }, "Damping",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, "%"));

    p.add (std::make_unique<FloatParam> (
        juce::ParameterID { "width", 1 }, "Width",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 78.0f, "%"));

    p.add (std::make_unique<FloatParam> (
        juce::ParameterID { "lowcut", 1 }, "Low Cut",
        juce::NormalisableRange<float> (20.0f, 1000.0f, 1.0f, 0.40f), 100.0f, " Hz"));

    p.add (std::make_unique<FloatParam> (
        juce::ParameterID { "highcut", 1 }, "High Cut",
        juce::NormalisableRange<float> (2000.0f, 20000.0f, 10.0f, 0.45f), 14000.0f, " Hz"));

    return p;
}

void DARoomAudioProcessor::setParameterValueWithGesture (juce::RangedAudioParameter* param, float plainValue)
{
    if (param == nullptr)
        return;

    const float norm = juce::jlimit (0.0f, 1.0f, param->convertTo0to1 (plainValue));
    param->beginChangeGesture();
    param->setValueNotifyingHost (norm);
    param->endChangeGesture();
}

void DARoomAudioProcessor::applyPreset (float mixPercent, float predelayMs, float decaySeconds, float sizePercent,
                                        int modeIndex, float diffusion, float modulation, float damping,
                                        float width, float lowCutHz, float highCutHz)
{
    const std::array<std::pair<const char*, float>, 10> vals {{
        { "mix", mixPercent },
        { "predelay", predelayMs },
        { "decay", decaySeconds },
        { "size", sizePercent },
        { "diffusion", diffusion },
        { "modulation", modulation },
        { "damping", damping },
        { "width", width },
        { "lowcut", lowCutHz },
        { "highcut", highCutHz }
    }};

    for (const auto& v : vals)
        setParameterValueWithGesture (parameters.getParameter (v.first), v.second);

    setMode (modeIndex);
}

void DARoomAudioProcessor::applyFactoryPreset (int index)
{
    if (index < 0 || index >= DARoomPresets::numFactoryPresets)
        return;

    currentPresetIndex = index;
    customLoadedPresetName.clear();
    const auto& pr = DARoomPresets::factoryPresets[index];
    applyPreset (pr.mix, pr.predelay, pr.decay, pr.size, pr.mode,
                 pr.diffusion, pr.modulation, pr.damping, pr.width, pr.lowCut, pr.highCut);
}

void DARoomAudioProcessor::setMode (int modeIndex)
{
    setParameterValueWithGesture (parameters.getParameter ("mode"),
                                  static_cast<float> (juce::jlimit (0, 4, modeIndex)));
}

int DARoomAudioProcessor::getMode() const noexcept
{
    if (const auto* raw = parameters.getRawParameterValue ("mode"))
        return juce::jlimit (0, 4, static_cast<int> (std::lround (raw->load())));
    return 3;
}

int DARoomAudioProcessor::getNumPrograms()
{
    return DARoomPresets::numFactoryPresets;
}

int DARoomAudioProcessor::getCurrentProgram()
{
    return juce::jlimit (0, DARoomPresets::numFactoryPresets - 1, currentPresetIndex);
}

void DARoomAudioProcessor::setCurrentProgram (int index)
{
    applyFactoryPreset (index);
}

const juce::String DARoomAudioProcessor::getProgramName (int index)
{
    if (index >= 0 && index < DARoomPresets::numFactoryPresets)
        return DARoomPresets::factoryPresets[index].name;
    return {};
}

void DARoomAudioProcessor::changeProgramName (int /*index*/, const juce::String& /*newName*/)
{
}

juce::String DARoomAudioProcessor::getCurrentPresetDisplayName() const
{
    if (customLoadedPresetName.isNotEmpty())
        return customLoadedPresetName;

    if (currentPresetIndex >= 0 && currentPresetIndex < DARoomPresets::numFactoryPresets)
        return DARoomPresets::factoryPresets[currentPresetIndex].name;

    return "Deep Chamber";
}

void DARoomAudioProcessor::captureSnapshot (std::array<float, 11>& dest) const
{
    for (size_t i = 0; i < dest.size(); ++i)
    {
        if (const auto* raw = parameters.getRawParameterValue (DARoomPresets::allParameterIDs[i]))
            dest[i] = raw->load();
    }
}

void DARoomAudioProcessor::restoreSnapshot (const std::array<float, 11>& src)
{
    for (size_t i = 0; i < src.size(); ++i)
        setParameterValueWithGesture (parameters.getParameter (DARoomPresets::allParameterIDs[i]), src[i]);
}

void DARoomAudioProcessor::switchAB (int targetSlot)
{
    const int clamped = juce::jlimit (0, 1, targetSlot);
    if (clamped == activeAB)
        return;

    if (activeAB == 0)
    {
        captureSnapshot (snapshotA);
        restoreSnapshot (snapshotB);
    }
    else
    {
        captureSnapshot (snapshotB);
        restoreSnapshot (snapshotA);
    }
    activeAB = clamped;
}

void DARoomAudioProcessor::copyActiveABToOther()
{
    if (activeAB == 0)
    {
        captureSnapshot (snapshotA);
        snapshotB = snapshotA;
    }
    else
    {
        captureSnapshot (snapshotB);
        snapshotA = snapshotB;
    }
}

bool DARoomAudioProcessor::loadPresetFromFile (const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

    const auto parsed = juce::JSON::parse (file);
    const auto* rootObj = parsed.getDynamicObject();
    if (rootObj == nullptr)
        return false;

    const auto paramsVar = rootObj->getProperty ("parameters");
    const auto* paramsObj = paramsVar.getDynamicObject();
    if (paramsObj == nullptr)
        return false;

    auto readFloat = [paramsObj] (const char* key, float fallback) -> float
    {
        const auto v = paramsObj->getProperty (key);
        return (v.isDouble() || v.isInt() || v.isInt64()) ? static_cast<float> (v) : fallback;
    };

    int modeIdx = 3;
    const auto modeVar = paramsObj->getProperty ("mode");
    if (modeVar.isString())
    {
        const auto modeStr = modeVar.toString().trim();
        for (int i = 0; i < 5; ++i)
        {
            if (modeStr.equalsIgnoreCase (DARoomPresets::modeJsonNames[i]))
            {
                modeIdx = i;
                break;
            }
        }
    }
    else if (modeVar.isInt() || modeVar.isDouble())
    {
        modeIdx = juce::jlimit (0, 4, static_cast<int> (modeVar));
    }

    applyPreset (
        readFloat ("mix", 26.0f),
        readFloat ("predelay_ms", 25.0f),
        readFloat ("decay_seconds", 4.5f),
        readFloat ("size_percent", 78.0f),
        modeIdx,
        readFloat ("diffusion_percent", 82.0f),
        readFloat ("modulation_percent", 18.0f),
        readFloat ("damping_percent", 50.0f),
        readFloat ("width_percent", 78.0f),
        readFloat ("low_cut_hz", 100.0f),
        readFloat ("high_cut_hz", 14000.0f));

    const auto nameVar = rootObj->getProperty ("name");
    customLoadedPresetName = nameVar.isString() && nameVar.toString().isNotEmpty()
                                 ? nameVar.toString()
                                 : file.getFileNameWithoutExtension();
    return true;
}

bool DARoomAudioProcessor::savePresetToFile (const juce::File& file, const juce::String& customName)
{
    auto* rootObj = new juce::DynamicObject();
    rootObj->setProperty ("format", "DARoomPreset");
    rootObj->setProperty ("version", 2);

    const int modeIdx = getMode();
    rootObj->setProperty ("category", juce::String (DARoomPresets::modeJsonNames[modeIdx]));
    const juce::String presetName = customName.isNotEmpty()
                                        ? customName
                                        : file.getFileNameWithoutExtension();
    rootObj->setProperty ("name", presetName);

    auto* paramsObj = new juce::DynamicObject();
    paramsObj->setProperty ("mix", static_cast<double> (parameters.getRawParameterValue ("mix")->load()));
    paramsObj->setProperty ("predelay_ms", static_cast<double> (parameters.getRawParameterValue ("predelay")->load()));
    paramsObj->setProperty ("decay_seconds", static_cast<double> (parameters.getRawParameterValue ("decay")->load()));
    paramsObj->setProperty ("size_percent", static_cast<double> (parameters.getRawParameterValue ("size")->load()));
    paramsObj->setProperty ("mode", juce::String (DARoomPresets::modeJsonNames[modeIdx]));
    paramsObj->setProperty ("diffusion_percent", static_cast<double> (parameters.getRawParameterValue ("diffusion")->load()));
    paramsObj->setProperty ("modulation_percent", static_cast<double> (parameters.getRawParameterValue ("modulation")->load()));
    paramsObj->setProperty ("damping_percent", static_cast<double> (parameters.getRawParameterValue ("damping")->load()));
    paramsObj->setProperty ("width_percent", static_cast<double> (parameters.getRawParameterValue ("width")->load()));
    paramsObj->setProperty ("low_cut_hz", static_cast<double> (parameters.getRawParameterValue ("lowcut")->load()));
    paramsObj->setProperty ("high_cut_hz", static_cast<double> (parameters.getRawParameterValue ("highcut")->load()));

    rootObj->setProperty ("parameters", juce::var (paramsObj));
    const juce::String jsonText = juce::JSON::toString (juce::var (rootObj), false);
    if (file.replaceWithText (jsonText))
    {
        customLoadedPresetName = presetName;
        return true;
    }
    return false;
}

void DARoomAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax (8000.0, sampleRate);

    // Circular predelay buffer up to 300 ms
    const int predelayMaxSamples = juce::jmax (32, static_cast<int> (std::ceil (currentSampleRate * 0.30)) + 32);
    predelayBuffer.setSize (2, predelayMaxSamples);
    predelayBuffer.clear();
    predelayWritePosition = 0;

    // Early reflections + late-bloom offset stereo buffer up to 150 ms
    const int erMaxSamples = juce::jmax (32, static_cast<int> (std::ceil (currentSampleRate * 0.15)) + 32);
    erBuffer.setSize (2, erMaxSamples);
    erBuffer.clear();
    erWritePosition = 0;

    // 4-stage stereo input allpass diffusers (up to 45 ms per stage)
    const int maxInputApSamples = juce::jmax (32, static_cast<int> (std::ceil (currentSampleRate * 0.045)) + 16);
    for (size_t s = 0; s < 4; ++s)
    {
        diffuserL[s].prepare (maxInputApSamples);
        diffuserR[s].prepare (maxInputApSamples);
    }

    // 8x8 Hadamard FDN tank lines (up to 180 ms per line for large Ambience/Hall + modulation excursion)
    const int maxFdnSamples = juce::jmax (64, static_cast<int> (std::ceil (currentSampleRate * 0.18)) + 32);
    const int maxTankApSamples = juce::jmax (32, static_cast<int> (std::ceil (currentSampleRate * 0.015)) + 16);

    const int initMode = getMode();
    const float initSize = juce::jlimit (0.1f, 1.0f, parameters.getRawParameterValue ("size")->load() * 0.01f);
    const float sizeScale = 0.36f + 0.74f * initSize;

    for (size_t k = 0; k < kNumFDNLines; ++k)
    {
        fdnLines[k].prepare (maxFdnSamples);
        const float initMs = kModeProfiles[initMode].baseMinDelayMs * kFDNRatios[k] * sizeScale;
        fdnLines[k].smoothedDelay = initMs * 0.001f * static_cast<float> (currentSampleRate);
        tankAllpasses[k].prepare (maxTankApSamples);
    }

    shimmer.prepare (currentSampleRate);
    lfoPhase = 0.0;
    inHpStateL = inHpStateR = 0.0f;
    inLpStateL = inLpStateR = 0.0f;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = currentSampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (juce::jmax (1, samplesPerBlock));
    spec.numChannels = 2;

    lowCutFilter.prepare (spec);
    lowCutFilter.reset();
    lowCutFilter.setType (juce::dsp::StateVariableTPTFilterType::highpass);

    highCutFilter.prepare (spec);
    highCutFilter.reset();
    highCutFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

    lastLowCutHz = -1.0f;
    lastHighCutHz = -1.0f;

    wetSmoother.reset (currentSampleRate, 0.025);
    drySmoother.reset (currentSampleRate, 0.025);
    predelaySamplesSmoother.reset (currentSampleRate, 0.045);
    widthSmoother.reset (currentSampleRate, 0.030);
    erGainSmoother.reset (currentSampleRate, 0.035);
    lateGainSmoother.reset (currentSampleRate, 0.035);
    shimmerGainSmoother.reset (currentSampleRate, 0.050);

    const float mix = parameters.getRawParameterValue ("mix")->load() * 0.01f;
    drySmoother.setCurrentAndTargetValue (std::cos (mix * juce::MathConstants<float>::halfPi));
    wetSmoother.setCurrentAndTargetValue (std::sin (mix * juce::MathConstants<float>::halfPi));

    const float predelayMs = parameters.getRawParameterValue ("predelay")->load();
    predelaySamplesSmoother.setCurrentAndTargetValue (predelayMs * 0.001f * static_cast<float> (currentSampleRate));

    const float widthNorm = parameters.getRawParameterValue ("width")->load() * 0.01f;
    widthSmoother.setCurrentAndTargetValue (widthNorm * 1.25f * kModeProfiles[initMode].stereoSpreadBias);

    erGainSmoother.setCurrentAndTargetValue (kModeProfiles[initMode].erGain);
    lateGainSmoother.setCurrentAndTargetValue (kModeProfiles[initMode].lateGain);
    shimmerGainSmoother.setCurrentAndTargetValue (kModeProfiles[initMode].shimmerSend);
}

void DARoomAudioProcessor::releaseResources()
{
    predelayBuffer.setSize (0, 0);
    erBuffer.setSize (0, 0);
}

bool DARoomAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo())
        return false;

    return in.size() <= out.size();
}

void DARoomAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int totalCh = buffer.getNumChannels();
    const int samples = buffer.getNumSamples();

    if (samples <= 0 || numIn < 1 || numOut < 1 || totalCh < 1)
        return;

    if (predelayBuffer.getNumSamples() < 8 || erBuffer.getNumSamples() < 8)
        return;

    const float mix = juce::jlimit (0.0f, 1.0f, parameters.getRawParameterValue ("mix")->load() * 0.01f);
    const float predelayMs = parameters.getRawParameterValue ("predelay")->load();
    const float decaySec = juce::jlimit (0.2f, 12.0f, parameters.getRawParameterValue ("decay")->load());
    const float sizeNorm = juce::jlimit (0.1f, 1.0f, parameters.getRawParameterValue ("size")->load() * 0.01f);
    const int mode = juce::jlimit (0, 4, static_cast<int> (std::lround (parameters.getRawParameterValue ("mode")->load())));
    const float diffusion = juce::jlimit (0.0f, 1.0f, parameters.getRawParameterValue ("diffusion")->load() * 0.01f);
    const float modulation = juce::jlimit (0.0f, 1.0f, parameters.getRawParameterValue ("modulation")->load() * 0.01f);
    const float damping = juce::jlimit (0.0f, 1.0f, parameters.getRawParameterValue ("damping")->load() * 0.01f);
    const float width = juce::jlimit (0.0f, 1.0f, parameters.getRawParameterValue ("width")->load() * 0.01f);
    const float lowCutHz = parameters.getRawParameterValue ("lowcut")->load();
    const float highCutHz = parameters.getRawParameterValue ("highcut")->load();

    const ModeAcousticProfile& prof = kModeProfiles[mode];
    const double safeRate = juce::jmax (8000.0, currentSampleRate);
    const float srF = static_cast<float> (safeRate);
    const float msToSamples = 0.001f * srF;

    // Post-tank user Low Cut & High Cut TPT filters
    const float clampedLowCut = juce::jlimit (20.0f, srF * 0.42f, lowCutHz);
    const float clampedHighCut = juce::jlimit (juce::jmax (250.0f, clampedLowCut + 100.0f),
                                               srF * 0.47f, highCutHz);
    if (std::abs (clampedLowCut - lastLowCutHz) > 0.25f)
    {
        lowCutFilter.setCutoffFrequency (clampedLowCut);
        lastLowCutHz = clampedLowCut;
    }
    if (std::abs (clampedHighCut - lastHighCutHz) > 0.5f)
    {
        highCutFilter.setCutoffFrequency (clampedHighCut);
        lastHighCutHz = clampedHighCut;
    }

    // Mode input voicing 1-pole coefficients
    const float inHpCoeff = 1.0f - std::exp (-juce::MathConstants<float>::twoPi
                                             * juce::jlimit (20.0f, 400.0f, prof.inputHpHz) / srF);
    const float inLpCoeff = 1.0f - std::exp (-juce::MathConstants<float>::twoPi
                                             * juce::jlimit (1500.0f, srF * 0.46f, prof.inputLpHz) / srF);

    // Equal-power wet/dry mix & smoothers
    drySmoother.setTargetValue (std::cos (mix * juce::MathConstants<float>::halfPi));
    wetSmoother.setTargetValue (std::sin (mix * juce::MathConstants<float>::halfPi));

    const int predelayLen = predelayBuffer.getNumSamples();
    predelaySamplesSmoother.setTargetValue (
        juce::jlimit (0.0f, static_cast<float> (predelayLen - 4), predelayMs * msToSamples));

    widthSmoother.setTargetValue (width * 1.25f * prof.stereoSpreadBias);
    erGainSmoother.setTargetValue (prof.erGain);
    lateGainSmoother.setTargetValue (prof.lateGain);
    shimmerGainSmoother.setTargetValue (prof.shimmerSend);

    // Early reflection tap offsets (scaled by Size) & late pre-bloom offset
    const int erLen = erBuffer.getNumSamples();
    const float erSizeScale = 0.50f + 0.65f * sizeNorm;
    int erTapsL[6], erTapsR[6];
    for (int t = 0; t < 6; ++t)
    {
        erTapsL[t] = juce::jlimit (1, erLen - 3, static_cast<int> (std::lround (prof.erTapMsL[t] * erSizeScale * msToSamples)));
        erTapsR[t] = juce::jlimit (1, erLen - 3, static_cast<int> (std::lround (prof.erTapMsR[t] * erSizeScale * msToSamples)));
    }
    const int lateOffsetSamples = juce::jlimit (0, erLen - 3,
                                                static_cast<int> (std::lround (prof.lateOffsetMs * (0.65f + 0.35f * sizeNorm) * msToSamples)));

    // 4-stage stereo input allpass diffusion
    static constexpr float baseDiffMsL[4] = { 3.71f,  6.83f, 11.17f, 16.93f };
    static constexpr float baseDiffMsR[4] = { 4.19f,  7.57f, 12.29f, 18.31f };
    const float diffScale = (0.45f + 0.65f * sizeNorm) * prof.diffTimeScale * msToSamples;
    int diffTapL[4], diffTapR[4];
    for (int s = 0; s < 4; ++s)
    {
        diffTapL[s] = juce::jmax (2, static_cast<int> (std::lround (baseDiffMsL[s] * diffScale)));
        diffTapR[s] = juce::jmax (2, static_cast<int> (std::lround (baseDiffMsR[s] * diffScale)));
    }
    const float diffFb = juce::jlimit (0.0f, 0.75f,
                                       prof.diffMinFeedback * 0.35f + diffusion * (0.42f + 0.32f * prof.diffMinFeedback));

    // 8x8 FDN target line delays and Jot 3-Band Proportional RT60 feedback gains (Low / Mid / High)
    const float fdnSizeScale = 0.36f + 0.74f * sizeNorm;
    const float effectiveRt60 = juce::jlimit (0.14f, 18.0f, decaySec * prof.decayMultiplier);
    const float lfRt60 = juce::jlimit (0.08f, 20.0f, effectiveRt60 * prof.lfRt60Ratio);
    const float dampScale = 1.35f - 1.12f * damping;
    const float hfRt60 = juce::jlimit (0.06f, 16.0f, effectiveRt60 * prof.hfRt60Ratio * dampScale);

    float targetFdnDelay[kNumFDNLines];
    float gLow[kNumFDNLines];
    float gMid[kNumFDNLines];
    float gHigh[kNumFDNLines];
    int tankApTap[kNumFDNLines];

    float meanDelaySec = 0.0f;
    for (size_t k = 0; k < kNumFDNLines; ++k)
    {
        const float lineMs = prof.baseMinDelayMs * kFDNRatios[k] * fdnSizeScale;
        const float lineSec = lineMs * 0.001f;
        meanDelaySec += lineSec * 0.125f;

        targetFdnDelay[k] = juce::jmax (8.0f, lineMs * msToSamples);
        gLow[k]  = juce::jlimit (0.005f, 0.988f, std::pow (10.0f, -3.0f * lineSec / lfRt60));
        gMid[k]  = juce::jlimit (0.005f, 0.988f, std::pow (10.0f, -3.0f * lineSec / effectiveRt60));
        gHigh[k] = juce::jlimit (0.005f, 0.988f, std::pow (10.0f, -3.0f * lineSec / hfRt60));
        tankApTap[k] = juce::jmax (2, static_cast<int> (std::lround (kTankApMs[k] * (0.55f + 0.55f * sizeNorm) * msToSamples)));
    }

    const float tankApFb = juce::jlimit (0.0f, 0.62f, 0.18f + 0.40f * diffusion);

    // Per-sample delay smoothing coefficient (~55 ms glide for click-free mode & size transitions)
    const float delayGlideCoeff = 1.0f - std::exp (-1.0f / (0.055f * srF));

    // Complementary 3-band crossover frequencies inside FDN loop (200 Hz LF / 2.8 kHz HF)
    const float lfCrossAlpha = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 200.0f / srF);
    const float hfCrossAlpha = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 2800.0f / srF);

    // Jot steady-state power compensation so short Rooms and long Halls share matched musical loudness
    const float tankInputGain = juce::jlimit (0.16f, 0.72f,
                                              1.93f * std::sqrt (meanDelaySec / juce::jmax (0.18f, effectiveRt60)));

    // Multi-phase tank LFO modulation (8 evenly spaced phases 0 .. 7*pi/4)
    const double lfoFreq = static_cast<double> (prof.modRateHz * (0.65f + 0.85f * modulation));
    const double lfoPhaseInc = juce::MathConstants<double>::twoPi * lfoFreq / safeRate;
    const float modExcursionSamples = (0.08f + 1.85f * modulation) * prof.modDepthScale * msToSamples;

    // Shimmer octave lowpass (~4.2 kHz)
    const float shimmerLpAlpha = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 4200.0f / srF);

    float* ch0 = buffer.getWritePointer (0);
    float* ch1 = (totalCh > 1) ? buffer.getWritePointer (1) : nullptr;
    const bool stereoInput = (numIn > 1 && ch1 != nullptr);

    float* preL = predelayBuffer.getWritePointer (0);
    float* preR = predelayBuffer.getWritePointer (1);
    float* erBufL = erBuffer.getWritePointer (0);
    float* erBufR = erBuffer.getWritePointer (1);

    for (int i = 0; i < samples; ++i)
    {
        const float inL = ch0[i];
        const float inR = stereoInput ? ch1[i] : inL;

        // 1. Smoothed fractional stereo predelay
        const float smoothPre = predelaySamplesSmoother.getNextValue();
        preL[predelayWritePosition] = inL;
        preR[predelayWritePosition] = inR;

        float delayedL = inL;
        float delayedR = inR;
        if (smoothPre > 0.001f)
        {
            const int dInt = static_cast<int> (smoothPre);
            const float dFrac = smoothPre - static_cast<float> (dInt);
            int r0 = predelayWritePosition - dInt;
            if (r0 < 0)
                r0 += predelayLen;
            int r1 = r0 - 1;
            if (r1 < 0)
                r1 += predelayLen;

            delayedL = preL[r0] * (1.0f - dFrac) + preL[r1] * dFrac;
            delayedR = preR[r0] * (1.0f - dFrac) + preR[r1] * dFrac;
        }
        if (++predelayWritePosition >= predelayLen)
            predelayWritePosition = 0;

        // 2. Mode-specific pre-tank acoustic voicing (HP + LP)
        inHpStateL += inHpCoeff * (delayedL - inHpStateL);
        inHpStateR += inHpCoeff * (delayedR - inHpStateR);
        const float hpOutL = delayedL - inHpStateL;
        const float hpOutR = delayedR - inHpStateR;

        inLpStateL += inLpCoeff * (hpOutL - inLpStateL);
        inLpStateR += inLpCoeff * (hpOutR - inLpStateR);
        const float voicedL = inLpStateL;
        const float voicedR = inLpStateR;

        // 3. Dedicated 12-tap stereo Early Reflections + Late pre-bloom offset
        erBufL[erWritePosition] = voicedL;
        erBufR[erWritePosition] = voicedR;

        float erOutL = 0.0f;
        float erOutR = 0.0f;
        for (int t = 0; t < 6; ++t)
        {
            int idxL = erWritePosition - erTapsL[t];
            if (idxL < 0)
                idxL += erLen;
            int idxR = erWritePosition - erTapsR[t];
            if (idxR < 0)
                idxR += erLen;

            const float w = kErTapWeights[t];
            // Subtle cross-feed on odd taps for natural 3D binaural width
            if ((t & 1) == 0)
            {
                erOutL += erBufL[idxL] * w;
                erOutR += erBufR[idxR] * w;
            }
            else
            {
                erOutL += (erBufL[idxL] * 0.78f + erBufR[idxR] * 0.22f) * w;
                erOutR += (erBufR[idxR] * 0.78f + erBufL[idxL] * 0.22f) * w;
            }
        }

        int lateIdx = erWritePosition - lateOffsetSamples;
        if (lateIdx < 0)
            lateIdx += erLen;
        float tankInL = erBufL[lateIdx];
        float tankInR = erBufR[lateIdx];

        if (++erWritePosition >= erLen)
            erWritePosition = 0;

        // 4. Optional crystalline +12st shimmer halo (active in Ambience mode, smoothed during mode switches)
        const float shimAmt = shimmerGainSmoother.getNextValue();
        if (shimAmt > 0.0005f)
        {
            float shL = 0.0f, shR = 0.0f;
            shimmer.process (tankInL, tankInR, shimmerLpAlpha, shL, shR);
            tankInL += shL * shimAmt;
            tankInR += shR * shimAmt;
        }

        // 5. 4-stage stereo allpass input diffusion
        float diffL = tankInL;
        float diffR = tankInR;
        if (diffFb > 0.001f)
        {
            for (size_t s = 0; s < 4; ++s)
            {
                diffL = diffuserL[s].process (diffL, diffTapL[s], diffFb);
                diffR = diffuserR[s].process (diffR, diffTapR[s], diffFb);
            }
        }

        // 6. 8x8 Orthogonal Walsh-Hadamard FDN Tank with 8-phase modulation & dual-band RT60 damping
        float lineRead[kNumFDNLines];
        for (size_t k = 0; k < kNumFDNLines; ++k)
        {
            auto& line = fdnLines[k];
            line.smoothedDelay += delayGlideCoeff * (targetFdnDelay[k] - line.smoothedDelay);

            const float phaseOffset = static_cast<float> (lfoPhase)
                                    + static_cast<float> (k) * (juce::MathConstants<float>::twoPi * 0.125f);
            const float modOffset = std::sin (phaseOffset) * modExcursionSamples;
            float x = line.readFractional (line.smoothedDelay + modOffset);

            // In-loop Schroeder allpass dispersion
            x = tankAllpasses[k].process (x, tankApTap[k], tankApFb);

            // Jot 3-Band Proportional RT60 Absorption (Low < 200 Hz, Mid 200..2800 Hz, High > 2800 Hz)
            line.lfLpState += lfCrossAlpha * (x - line.lfLpState);
            line.hfLpState += hfCrossAlpha * (x - line.hfLpState);
            const float lowBand  = line.lfLpState;
            const float midBand  = line.hfLpState - line.lfLpState;
            const float highBand = x - line.hfLpState;
            const float damped   = lowBand * gLow[k] + midBand * gMid[k] + highBand * gHigh[k];

            // Sub-sonic 5 Hz DC blocker
            const float dcOut = damped - line.dcX1 + 0.9993f * line.dcY1;
            line.dcX1 = damped;
            line.dcY1 = dcOut;

            lineRead[k] = dcOut;
        }

        // Fast 8-point Walsh-Hadamard Butterfly (exact orthogonal norm 1/sqrt(8))
        const float a0 = lineRead[0] + lineRead[1], a1 = lineRead[0] - lineRead[1];
        const float a2 = lineRead[2] + lineRead[3], a3 = lineRead[2] - lineRead[3];
        const float a4 = lineRead[4] + lineRead[5], a5 = lineRead[4] - lineRead[5];
        const float a6 = lineRead[6] + lineRead[7], a7 = lineRead[6] - lineRead[7];

        const float b0 = a0 + a2, b1 = a1 + a3, b2 = a0 - a2, b3 = a1 - a3;
        const float b4 = a4 + a6, b5 = a5 + a7, b6 = a4 - a6, b7 = a5 - a7;

        constexpr float kHadamardNorm = 0.35355339059327376f;
        const float mix0 = (b0 + b4) * kHadamardNorm;
        const float mix1 = (b1 + b5) * kHadamardNorm;
        const float mix2 = (b2 + b6) * kHadamardNorm;
        const float mix3 = (b3 + b7) * kHadamardNorm;
        const float mix4 = (b0 - b4) * kHadamardNorm;
        const float mix5 = (b1 - b5) * kHadamardNorm;
        const float mix6 = (b2 - b6) * kHadamardNorm;
        const float mix7 = (b3 - b7) * kHadamardNorm;

        const float injL = diffL * tankInputGain;
        const float injR = diffR * tankInputGain;

        fdnLines[0].writeSample (mix0 + injL);
        fdnLines[1].writeSample (mix1 + injR);
        fdnLines[2].writeSample (mix2 - injL * 0.75f + injR * 0.25f);
        fdnLines[3].writeSample (mix3 - injR * 0.75f + injL * 0.25f);
        fdnLines[4].writeSample (mix4 + injL * 0.75f + injR * 0.25f);
        fdnLines[5].writeSample (mix5 + injR * 0.75f + injL * 0.25f);
        fdnLines[6].writeSample (mix6 - injL * 0.50f - injR * 0.50f);
        fdnLines[7].writeSample (mix7 + injL * 0.50f - injR * 0.50f);

        // Decorrelated stereo tank tap across all 8 lines
        const float lateL = (lineRead[0] + lineRead[2] - lineRead[4] + lineRead[6]) * 0.55f;
        const float lateR = (lineRead[1] - lineRead[3] + lineRead[5] + lineRead[7]) * 0.55f;

        lfoPhase += lfoPhaseInc;
        if (lfoPhase >= juce::MathConstants<double>::twoPi)
            lfoPhase -= juce::MathConstants<double>::twoPi;

        const float erGainCur = erGainSmoother.getNextValue();
        const float lateGainCur = lateGainSmoother.getNextValue();

        float wetL = erOutL * erGainCur + lateL * lateGainCur;
        float wetR = erOutR * erGainCur + lateR * lateGainCur;

        // 7. User Low Cut & High Cut TPT filters
        wetL = highCutFilter.processSample (0, lowCutFilter.processSample (0, wetL));
        wetR = highCutFilter.processSample (1, lowCutFilter.processSample (1, wetR));

        // 8. Smoothed M/S Stereo Width
        const float sideGain = widthSmoother.getNextValue();
        const float mid = 0.5f * (wetL + wetR);
        const float side = 0.5f * (wetL - wetR) * sideGain;
        wetL = mid + side;
        wetR = mid - side;

        const float dry = drySmoother.getNextValue();
        const float wet = wetSmoother.getNextValue();

        if (numOut == 1 || ch1 == nullptr)
        {
            ch0[i] = inL * dry + 0.5f * (wetL + wetR) * wet;
        }
        else
        {
            ch0[i] = inL * dry + wetL * wet;
            ch1[i] = inR * dry + wetR * wet;
        }
    }

    for (int ch = juce::jmax (2, numOut); ch < totalCh; ++ch)
        buffer.clear (ch, 0, samples);
}

juce::AudioProcessorEditor* DARoomAudioProcessor::createEditor()
{
    return new DARoomAudioProcessorEditor (*this);
}

void DARoomAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml != nullptr)
    {
        xml->setAttribute ("presetIndex", currentPresetIndex);
        xml->setAttribute ("customPresetName", customLoadedPresetName);
        xml->setAttribute ("activeAB", activeAB);

        auto saveSnap = [&xml] (const char* tagName, const std::array<float, 11>& snap)
        {
            auto* child = xml->createNewChildElement (tagName);
            for (size_t i = 0; i < snap.size(); ++i)
                child->setAttribute (DARoomPresets::allParameterIDs[i], static_cast<double> (snap[i]));
        };
        saveSnap ("SnapshotA", snapshotA);
        saveSnap ("SnapshotB", snapshotB);

        copyXmlToBinary (*xml, destData);
    }
}

void DARoomAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (parameters.state.getType()))
    {
        const int savedPreset = xml->getIntAttribute ("presetIndex", currentPresetIndex);
        const juce::String savedCustomName = xml->getStringAttribute ("customPresetName", {});
        const int savedAB = juce::jlimit (0, 1, xml->getIntAttribute ("activeAB", 0));

        auto loadSnap = [&xml] (const char* tagName, std::array<float, 11>& snap) -> bool
        {
            if (const auto* child = xml->getChildByName (tagName))
            {
                for (size_t i = 0; i < snap.size(); ++i)
                    snap[i] = static_cast<float> (child->getDoubleAttribute (
                        DARoomPresets::allParameterIDs[i], static_cast<double> (snap[i])));
                return true;
            }
            return false;
        };

        const bool hadA = loadSnap ("SnapshotA", snapshotA);
        const bool hadB = loadSnap ("SnapshotB", snapshotB);

        xml->deleteAllChildElementsWithTagName ("SnapshotA");
        xml->deleteAllChildElementsWithTagName ("SnapshotB");

        parameters.replaceState (juce::ValueTree::fromXml (*xml));

        currentPresetIndex = juce::jlimit (0, DARoomPresets::numFactoryPresets - 1, savedPreset);
        customLoadedPresetName = savedCustomName;
        activeAB = savedAB;

        if (! hadA)
            captureSnapshot (snapshotA);
        if (! hadB)
            snapshotB = snapshotA;
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DARoomAudioProcessor();
}
