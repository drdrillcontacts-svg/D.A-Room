#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

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

    reverb.reset();
    reverb.setSampleRate (currentSampleRate);

    // Circular predelay ring buffer up to 300 ms
    const int predelayMaxSamples = juce::jmax (16, static_cast<int> (std::ceil (currentSampleRate * 0.30)) + 16);
    predelayBuffer.setSize (2, predelayMaxSamples);
    predelayBuffer.clear();
    predelayWritePosition = 0;

    // Circular stereo quadrature chorus modulation buffer up to 30 ms
    const int modMaxSamples = juce::jmax (16, static_cast<int> (std::ceil (currentSampleRate * 0.030)) + 16);
    modulationBuffer.setSize (2, modMaxSamples);
    modulationBuffer.clear();
    modulationWritePosition = 0;
    modulationPhase = 0.0f;

    // 4-stage stereo allpass input diffusion lines (up to ~35 ms per stage)
    const int maxAllpassSamples = juce::jmax (32, static_cast<int> (std::ceil (currentSampleRate * 0.035)) + 16);
    for (size_t s = 0; s < 4; ++s)
    {
        diffuserL[s].prepare (maxAllpassSamples);
        diffuserR[s].prepare (maxAllpassSamples);
    }

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
    predelaySamplesSmoother.reset (currentSampleRate, 0.040);
    widthSmoother.reset (currentSampleRate, 0.025);

    const float mix = parameters.getRawParameterValue ("mix")->load() * 0.01f;
    const float dryTarget = std::cos (mix * juce::MathConstants<float>::halfPi);
    const float wetTarget = std::sin (mix * juce::MathConstants<float>::halfPi);
    drySmoother.setCurrentAndTargetValue (dryTarget);
    wetSmoother.setCurrentAndTargetValue (wetTarget);

    const float predelayMs = parameters.getRawParameterValue ("predelay")->load();
    predelaySamplesSmoother.setCurrentAndTargetValue (predelayMs * 0.001f * static_cast<float> (currentSampleRate));

    const float widthNorm = parameters.getRawParameterValue ("width")->load() * 0.01f;
    widthSmoother.setCurrentAndTargetValue (widthNorm * 1.25f);
}

void DARoomAudioProcessor::releaseResources()
{
    predelayBuffer.setSize (0, 0);
    modulationBuffer.setSize (0, 0);
}

bool DARoomAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo())
        return false;

    // Support Mono->Mono, Mono->Stereo, and Stereo->Stereo
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

    if (predelayBuffer.getNumSamples() < 4 || modulationBuffer.getNumSamples() < 4)
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

    // Mode acoustic character profiles: Room, Hall, Plate, Chamber, Ambience
    static constexpr float modeDecayBias[5]   = { -0.05f,  0.03f,  0.01f,  0.02f,  0.06f };
    static constexpr float modeDampingBias[5] = {  0.14f,  0.00f, -0.14f,  0.04f,  0.08f };
    static constexpr float modeDiffScale[5]   = {  0.58f,  1.05f,  0.48f,  0.82f,  1.32f };

    // Perceptual decay curve mapping 0.20s..12.00s cleanly across roomSize feedback range
    const float decayShaped = std::pow ((decaySec - 0.2f) / 11.8f, 0.42f);
    reverbParams.roomSize = juce::jlimit (
        0.08f, 0.985f,
        0.10f + decayShaped * 0.68f + sizeNorm * 0.16f + modeDecayBias[mode]);
    reverbParams.damping = juce::jlimit (
        0.02f, 0.97f,
        damping * 0.82f + 0.12f + modeDampingBias[mode]);
    reverbParams.wetLevel = 1.0f;
    reverbParams.dryLevel = 0.0f;
    reverbParams.width = 1.0f; // Keep tank stereo decorrelation intact; output width controlled once via M/S
    reverbParams.freezeMode = 0.0f;
    reverb.setParameters (reverbParams);

    const double safeRate = juce::jmax (8000.0, currentSampleRate);
    const float clampedLowCut = juce::jlimit (20.0f, static_cast<float> (safeRate * 0.42), lowCutHz);
    const float clampedHighCut = juce::jlimit (juce::jmax (250.0f, clampedLowCut + 100.0f),
                                               static_cast<float> (safeRate * 0.47), highCutHz);

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

    // Equal-power / natural studio mix law
    drySmoother.setTargetValue (std::cos (mix * juce::MathConstants<float>::halfPi));
    wetSmoother.setTargetValue (std::sin (mix * juce::MathConstants<float>::halfPi));

    const int predelayLength = predelayBuffer.getNumSamples();
    const float targetDelaySamples = juce::jlimit (
        0.0f, static_cast<float> (predelayLength - 3),
        predelayMs * 0.001f * static_cast<float> (safeRate));
    predelaySamplesSmoother.setTargetValue (targetDelaySamples);

    // Stereo width: 0% -> mono (0.0), ~78% -> natural 1.0x stereo, 100% -> wide 1.28x M/S
    widthSmoother.setTargetValue (width * 1.28f);

    // Prime-ratio stereo allpass tap times (in ms) scaled by Size & Mode
    static constexpr float baseTapMsL[4] = { 4.37f,  7.31f, 11.73f, 17.89f };
    static constexpr float baseTapMsR[4] = { 4.91f,  8.09f, 12.61f, 19.13f };
    const float tapScale = (0.45f + 0.55f * sizeNorm) * modeDiffScale[mode] * 0.001f * static_cast<float> (safeRate);
    int tapL[4], tapR[4];
    for (int s = 0; s < 4; ++s)
    {
        tapL[s] = juce::jmax (2, static_cast<int> (std::lround (baseTapMsL[s] * tapScale)));
        tapR[s] = juce::jmax (2, static_cast<int> (std::lround (baseTapMsR[s] * tapScale)));
    }
    const float diffFeedback = 0.66f * diffusion;

    const int modLength = modulationBuffer.getNumSamples();
    const float lfoRate = 0.21f + modulation * 0.92f;
    const float modDepth = modulation * static_cast<float> (safeRate) * 0.0042f;
    const float phaseInc = static_cast<float> (juce::MathConstants<double>::twoPi * lfoRate / safeRate);

    float* ch0 = buffer.getWritePointer (0);
    float* ch1 = (totalCh > 1) ? buffer.getWritePointer (1) : nullptr;
    const bool stereoInput = (numIn > 1 && ch1 != nullptr);

    float* preL = predelayBuffer.getWritePointer (0);
    float* preR = predelayBuffer.getWritePointer (1);
    float* modBufL = modulationBuffer.getWritePointer (0);
    float* modBufR = modulationBuffer.getWritePointer (1);

    for (int i = 0; i < samples; ++i)
    {
        const float inL = ch0[i];
        const float inR = stereoInput ? ch1[i] : inL;

        // Smoothed fractional predelay
        const float smoothDelay = predelaySamplesSmoother.getNextValue();
        preL[predelayWritePosition] = inL;
        preR[predelayWritePosition] = inR;

        float delayedL = inL;
        float delayedR = inR;
        if (smoothDelay > 0.001f)
        {
            const int dInt = static_cast<int> (smoothDelay);
            const float dFrac = smoothDelay - static_cast<float> (dInt);
            int r0 = predelayWritePosition - dInt;
            if (r0 < 0)
                r0 += predelayLength;
            int r1 = r0 - 1;
            if (r1 < 0)
                r1 += predelayLength;

            delayedL = preL[r0] * (1.0f - dFrac) + preL[r1] * dFrac;
            delayedR = preR[r0] * (1.0f - dFrac) + preR[r1] * dFrac;
        }

        if (++predelayWritePosition >= predelayLength)
            predelayWritePosition = 0;

        // 4-stage stereo allpass input diffusion
        float diffL = delayedL;
        float diffR = delayedR;
        if (diffFeedback > 0.001f)
        {
            for (size_t s = 0; s < 4; ++s)
            {
                diffL = diffuserL[s].process (diffL, tapL[s], diffFeedback);
                diffR = diffuserR[s].process (diffR, tapR[s], diffFeedback);
            }
        }

        // Stereo reverb tank
        float wetL = diffL;
        float wetR = diffR;
        reverb.processStereo (&wetL, &wetR, 1);

        // Quadrature stereo chorus modulation (90-degree L/R LFO offset prevents mono warble)
        modBufL[modulationWritePosition] = wetL;
        modBufR[modulationWritePosition] = wetR;

        if (modDepth > 0.01f)
        {
            const float depthL = 1.0f + modDepth * (0.5f + 0.5f * std::sin (modulationPhase));
            const float depthR = 1.0f + modDepth * (0.5f + 0.5f * std::cos (modulationPhase));

            const float cL = juce::jlimit (1.0f, static_cast<float> (modLength - 3), depthL);
            const int iL = static_cast<int> (cL);
            const float fL = cL - static_cast<float> (iL);
            int raL = modulationWritePosition - iL;
            if (raL < 0)
                raL += modLength;
            int rbL = raL - 1;
            if (rbL < 0)
                rbL += modLength;
            wetL = modBufL[raL] * (1.0f - fL) + modBufL[rbL] * fL;

            const float cR = juce::jlimit (1.0f, static_cast<float> (modLength - 3), depthR);
            const int iR = static_cast<int> (cR);
            const float fR = cR - static_cast<float> (iR);
            int raR = modulationWritePosition - iR;
            if (raR < 0)
                raR += modLength;
            int rbR = raR - 1;
            if (rbR < 0)
                rbR += modLength;
            wetR = modBufR[raR] * (1.0f - fR) + modBufR[rbR] * fR;
        }

        if (++modulationWritePosition >= modLength)
            modulationWritePosition = 0;

        modulationPhase += phaseInc;
        if (modulationPhase >= juce::MathConstants<float>::twoPi)
            modulationPhase -= juce::MathConstants<float>::twoPi;

        // Zero-allocation stereo TPT Low Cut & High Cut filters
        wetL = highCutFilter.processSample (0, lowCutFilter.processSample (0, wetL));
        wetR = highCutFilter.processSample (1, lowCutFilter.processSample (1, wetR));

        // Single-stage smoothed M/S Stereo Width
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

        // Remove custom metadata child nodes before replacing APVTS ValueTree
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
