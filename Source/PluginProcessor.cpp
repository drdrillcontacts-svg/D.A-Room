#include "PluginProcessor.h"
#include "PluginEditor.h"

DARoomAudioProcessor::DARoomAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "DARoomState", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout DARoomAudioProcessor::createParameterLayout()
{
    using FloatParam = juce::AudioParameterFloat;
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    p.add (std::make_unique<FloatParam>("mix", "Mix", juce::NormalisableRange<float>(0.0f,100.0f,0.1f),26.0f,"%"));
    p.add (std::make_unique<FloatParam>("predelay", "Predelay", juce::NormalisableRange<float>(0.0f,250.0f,0.1f),25.0f," ms"));
    p.add (std::make_unique<FloatParam>("decay", "Decay", juce::NormalisableRange<float>(0.2f,12.0f,0.01f),4.5f," s"));
    p.add (std::make_unique<FloatParam>("size", "Size", juce::NormalisableRange<float>(10.0f,100.0f,0.1f),78.0f,"%"));
    p.add (std::make_unique<juce::AudioParameterChoice>("mode","Reverb Mode",juce::StringArray{"Room","Hall","Plate","Chamber","Ambience"},3));
    p.add (std::make_unique<FloatParam>("diffusion","Diffusion",juce::NormalisableRange<float>(0.0f,100.0f,0.1f),82.0f,"%"));
    p.add (std::make_unique<FloatParam>("modulation","Modulation",juce::NormalisableRange<float>(0.0f,100.0f,0.1f),18.0f,"%"));
    p.add (std::make_unique<FloatParam>("damping","Damping",juce::NormalisableRange<float>(0.0f,100.0f,0.1f),50.0f,"%"));
    p.add (std::make_unique<FloatParam>("width","Width",juce::NormalisableRange<float>(0.0f,100.0f,0.1f),78.0f,"%"));
    p.add (std::make_unique<FloatParam>("lowcut","Low Cut",juce::NormalisableRange<float>(20.0f,1000.0f,1.0f),100.0f," Hz"));
    p.add (std::make_unique<FloatParam>("highcut","High Cut",juce::NormalisableRange<float>(2000.0f,20000.0f,10.0f),14000.0f," Hz"));
    return p;
}

void DARoomAudioProcessor::applyPreset(float mix,float predelay,float decay,float size,int mode,
                                       float diffusion,float modulation,float damping,float width,
                                       float lowCut,float highCut)
{
    const std::array<std::pair<const char*,float>,10> vals {{
        {"mix",mix},{"predelay",predelay},{"decay",decay},{"size",size},
        {"diffusion",diffusion},{"modulation",modulation},{"damping",damping},
        {"width",width},{"lowcut",lowCut},{"highcut",highCut}
    }};
    for (const auto& v : vals)
        if (auto* param=parameters.getParameter(v.first))
            param->setValueNotifyingHost(param->convertTo0to1(v.second));
    setMode(mode);
}

void DARoomAudioProcessor::setMode(int modeIndex)
{
    if (auto* param=parameters.getParameter("mode"))
        param->setValueNotifyingHost(param->convertTo0to1((float)juce::jlimit(0,4,modeIndex)));
}

void DARoomAudioProcessor::prepareToPlay(double sampleRate,int samplesPerBlock)
{
    currentSampleRate=sampleRate;
    reverb.reset();
    reverb.setSampleRate(sampleRate);
    predelayBuffer.setSize(2,juce::jmax(2,(int)std::ceil(sampleRate*0.30)+samplesPerBlock+4));
    predelayBuffer.clear(); predelayWritePosition=0;
    modulationBuffer.setSize(2,juce::jmax(2,(int)std::ceil(sampleRate*0.025)+samplesPerBlock+4));
    modulationBuffer.clear(); modulationWritePosition=0; modulationPhase=0.0f;
    wetSmoother.reset(sampleRate,0.025); drySmoother.reset(sampleRate,0.025);
    const float mix=parameters.getRawParameterValue("mix")->load()*0.01f;
    wetSmoother.setCurrentAndTargetValue(mix); drySmoother.setCurrentAndTargetValue(1.0f-mix);
    lowCutL.reset(); lowCutR.reset(); highCutL.reset(); highCutR.reset();
}

void DARoomAudioProcessor::releaseResources()
{
    predelayBuffer.setSize(0,0);
    modulationBuffer.setSize(0,0);
}

bool DARoomAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in=layouts.getMainInputChannelSet(), out=layouts.getMainOutputChannelSet();
    return in==out && (out==juce::AudioChannelSet::mono() || out==juce::AudioChannelSet::stereo());
}

void DARoomAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int channels=buffer.getNumChannels(), samples=buffer.getNumSamples();
    for(int ch=getTotalNumInputChannels();ch<getTotalNumOutputChannels();++ch) buffer.clear(ch,0,samples);
    if(channels<1 || predelayBuffer.getNumSamples()<2 || modulationBuffer.getNumSamples()<2) return;

    const float mix=parameters.getRawParameterValue("mix")->load()*0.01f;
    const float predelayMs=parameters.getRawParameterValue("predelay")->load();
    const float decay=parameters.getRawParameterValue("decay")->load();
    const float size=parameters.getRawParameterValue("size")->load()*0.01f;
    const int mode=juce::jlimit(0,4,(int)parameters.getRawParameterValue("mode")->load());
    const float diffusion=parameters.getRawParameterValue("diffusion")->load()*0.01f;
    const float modulation=parameters.getRawParameterValue("modulation")->load()*0.01f;
    const float damping=parameters.getRawParameterValue("damping")->load()*0.01f;
    const float width=parameters.getRawParameterValue("width")->load()*0.01f;
    const float lowCut=parameters.getRawParameterValue("lowcut")->load();
    const float highCut=parameters.getRawParameterValue("highcut")->load();

    const float modeSize[]={0.48f,0.78f,0.88f,0.68f,0.42f};
    const float modeDamping[]={0.70f,0.45f,0.24f,0.52f,0.74f};
    reverbParams.roomSize=juce::jlimit(0.05f,0.98f,0.10f+size*0.52f+diffusion*0.22f+(decay/12.0f)*0.12f+modeSize[mode]*0.08f);
    reverbParams.damping=juce::jlimit(0.0f,0.98f,damping*0.78f+modeDamping[mode]*0.22f);
    reverbParams.wetLevel=1.0f; reverbParams.dryLevel=0.0f;
    reverbParams.width=juce::jlimit(0.0f,1.0f,width); reverbParams.freezeMode=0.0f;
    reverb.setParameters(reverbParams);

    const double safeRate=juce::jmax(8000.0,currentSampleRate);
    auto lowCoeffs=juce::dsp::IIR::Coefficients<float>::makeHighPass(safeRate,juce::jlimit(20.0f, (float)(safeRate*0.40),lowCut));
    auto highCoeffs=juce::dsp::IIR::Coefficients<float>::makeLowPass(safeRate,juce::jlimit(200.0f,(float)(safeRate*0.48),highCut));
    lowCutL.coefficients=lowCoeffs; lowCutR.coefficients=lowCoeffs;
    highCutL.coefficients=highCoeffs; highCutR.coefficients=highCoeffs;

    wetSmoother.setTargetValue(mix); drySmoother.setTargetValue(1.0f-mix);
    const int predelayLength=predelayBuffer.getNumSamples();
    const int delaySamples=juce::jlimit(0,predelayLength-1,(int)std::round(predelayMs*0.001*currentSampleRate));
    const int modLength=modulationBuffer.getNumSamples();
    const float lfoRate=0.23f+modulation*0.87f;
    const float modDepth=modulation*(float)currentSampleRate*0.0045f;
    const float phaseInc=(float)(juce::MathConstants<double>::twoPi*lfoRate/safeRate);

    for(int i=0;i<samples;++i)
    {
        const float inL=buffer.getSample(0,i);
        const float inR=channels>1?buffer.getSample(1,i):inL;
        float delayedL=inL,delayedR=inR;
        if(delaySamples>0)
        {
            const int rp=(predelayWritePosition-delaySamples+predelayLength)%predelayLength;
            delayedL=predelayBuffer.getSample(0,rp); delayedR=predelayBuffer.getSample(1,rp);
        }
        predelayBuffer.setSample(0,predelayWritePosition,inL); predelayBuffer.setSample(1,predelayWritePosition,inR);
        predelayWritePosition=(predelayWritePosition+1)%predelayLength;

        float wetL=delayedL,wetR=delayedR;
        reverb.processStereo(wetL,wetR);

        const float localDepth=modDepth*(0.5f+0.5f*std::sin(modulationPhase));
        const float d=juce::jlimit(1.0f,(float)(modLength-2),1.0f+localDepth);
        const int di=(int)d; const float frac=d-(float)di;
        const int ra=(modulationWritePosition-di+modLength)%modLength;
        const int rb=(ra-1+modLength)%modLength;
        modulationBuffer.setSample(0,modulationWritePosition,wetL);
        modulationBuffer.setSample(1,modulationWritePosition,wetR);
        wetL=modulationBuffer.getSample(0,ra)*(1.0f-frac)+modulationBuffer.getSample(0,rb)*frac;
        wetR=modulationBuffer.getSample(1,ra)*(1.0f-frac)+modulationBuffer.getSample(1,rb)*frac;
        modulationWritePosition=(modulationWritePosition+1)%modLength;
        modulationPhase+=phaseInc;
        if(modulationPhase>(float)juce::MathConstants<double>::twoPi) modulationPhase-=(float)juce::MathConstants<double>::twoPi;

        wetL=highCutL.processSample(0,lowCutL.processSample(0,wetL));
        wetR=highCutR.processSample(0,lowCutR.processSample(0,wetR));
        const float mid=0.5f*(wetL+wetR), side=0.5f*(wetL-wetR)*width;
        wetL=mid+side; wetR=mid-side;

        const float wet=wetSmoother.getNextValue(), dry=drySmoother.getNextValue();
        buffer.setSample(0,i,inL*dry+wetL*wet);
        if(channels>1) buffer.setSample(1,i,inR*dry+wetR*wet);
    }
}

juce::AudioProcessorEditor* DARoomAudioProcessor::createEditor(){return new DARoomAudioProcessorEditor(*this);}
void DARoomAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state=parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml,destData);
}
void DARoomAudioProcessor::setStateInformation(const void* data,int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data,sizeInBytes));
    if(xml!=nullptr && xml->hasTagName(parameters.state.getType()))
        parameters.replaceState(juce::ValueTree::fromXml(*xml));
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new DARoomAudioProcessor();}
