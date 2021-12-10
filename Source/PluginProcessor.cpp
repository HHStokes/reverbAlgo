/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"


struct ChainSettings {
    //float dlFeedback{ 0 }, dlTime{ 0 };
    float reverb1Amount{ 0 }, reverb1Mix{ 0 }, reverb1ModRate{ 0 }, reverb1ModDepth{ 0 };
    float reverb2Amount{ 0 }, reverb2Mix{ 0 }, reverb2ModRate{ 0 }, reverb2ModDepth{ 0 };
    float masterHighpass{ 0 }, masterLowpass{ 0 }, masterDryWet{ 0 };
};
ChainSettings getChainSettings(juce::AudioProcessorValueTreeState&);
void linkChainSettings(ChainSettings);


//==============================================================================
MarsAudioProcessor::MarsAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : foleys::MagicProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    FOLEYS_SET_SOURCE_PATH(__FILE__);
}

MarsAudioProcessor::~MarsAudioProcessor()
{
}

//==============================================================================
const juce::String MarsAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool MarsAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool MarsAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool MarsAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double MarsAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int MarsAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int MarsAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MarsAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String MarsAudioProcessor::getProgramName (int index)
{
    return {};
}

void MarsAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void MarsAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    //setting the size of our buffer
    auto dlBufferSize = 2.0 * (sampleRate * samplesPerBlock);
    samplesPerSecond = sampleRate * samplesPerBlock;
    dlAudioBuffer.setSize(getTotalNumOutputChannels(), (int)dlBufferSize);
    dryBuffer.setSize(getTotalNumOutputChannels(), (int)dlBufferSize);
    dlAudioBuffer.clear();

    UniversalSampleRate = (float)sampleRate;

    //setting up the spec for dsp
    spec.sampleRate = UniversalSampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getTotalNumOutputChannels();

    leftChain.reset();
    rightChain.reset();

    leftChain.get<ChainPositions::Reverb1>().prepare(spec);
    rightChain.get<ChainPositions::Reverb1>().prepare(spec);
    leftChain.get<ChainPositions::Reverb2>().prepare(spec);
    rightChain.get<ChainPositions::Reverb2>().prepare(spec);
    leftChain.get<ChainPositions::Chorus1>().prepare(spec);
    rightChain.get<ChainPositions::Chorus1>().prepare(spec);
    leftChain.get<ChainPositions::Chorus2>().prepare(spec);
    rightChain.get<ChainPositions::Chorus2>().prepare(spec);
    leftChain.get<ChainPositions::LowPass>().prepare(spec);
    rightChain.get<ChainPositions::LowPass>().prepare(spec);
    leftChain.get<ChainPositions::HighPass>().prepare(spec);
    rightChain.get<ChainPositions::HighPass>().prepare(spec);

    auto chainSettings = getChainSettings(apvts);

    auto lowpassCoefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(UniversalSampleRate, chainSettings.masterLowpass);
    auto highpassCoefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(UniversalSampleRate, chainSettings.masterHighpass);

    *leftChain.get<ChainPositions::LowPass>().coefficients = *lowpassCoefficients;
    *rightChain.get<ChainPositions::LowPass>().coefficients = *lowpassCoefficients;

    *leftChain.get<ChainPositions::HighPass>().coefficients = *highpassCoefficients;
    *rightChain.get<ChainPositions::HighPass>().coefficients = *highpassCoefficients;
}


void MarsAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool MarsAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif


void MarsAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
        
    auto chainSettings = getChainSettings(apvts);

    //leftChain.get<ChainPositions::DryMix>().setMixingRule(juce::dsp::DryWetMixingRule::balanced);
    //rightChain.get<ChainPositions::DryMix>().setMixingRule(juce::dsp::DryWetMixingRule::balanced);
    //leftChain.get<ChainPositions::DryMix>().setWetMixProportion(chainSettings.masterDryWet);
    //rightChain.get<ChainPositions::DryMix>().setWetMixProportion(chainSettings.masterDryWet);

    /*for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer (channel);
    }*/

    reverb1Parameters.roomSize = chainSettings.reverb1Mix;
    reverb1Parameters.damping = 0.33f;
    reverb1Parameters.wetLevel = chainSettings.reverb1Mix;
    reverb1Parameters.dryLevel = 1.f + (-1.f * chainSettings.reverb1Mix);
    reverb1Parameters.freezeMode = chainSettings.reverb1Amount * 0.3f;

    leftChain.get<ChainPositions::Chorus1>().setFeedback(-0.2999f);
    rightChain.get<ChainPositions::Chorus1>().setFeedback(-0.3001f);
    leftChain.get<ChainPositions::Chorus1>().setMix(chainSettings.reverb1Mix * 0.33f);
    rightChain.get<ChainPositions::Chorus1>().setMix(chainSettings.reverb1Mix * 0.33f);
    leftChain.get<ChainPositions::Chorus1>().setDepth(chainSettings.reverb1ModDepth);
    rightChain.get<ChainPositions::Chorus1>().setDepth(chainSettings.reverb1ModDepth);
    leftChain.get<ChainPositions::Chorus1>().setRate(chainSettings.reverb1ModRate - 0.001);
    rightChain.get<ChainPositions::Chorus1>().setRate(chainSettings.reverb1ModRate);

    reverb2Parameters.roomSize = chainSettings.reverb2Mix;
    reverb2Parameters.damping = 0.71f;
    reverb2Parameters.wetLevel = chainSettings.reverb2Mix;
    reverb2Parameters.dryLevel = 1.f + (-1.f * chainSettings.reverb2Amount);
    reverb2Parameters.freezeMode = chainSettings.reverb2Amount * 0.3f;

    leftChain.get<ChainPositions::Chorus2>().setFeedback(0.2887f);
    rightChain.get<ChainPositions::Chorus2>().setFeedback(0.3112f);
    leftChain.get<ChainPositions::Chorus2>().setMix(chainSettings.reverb2Mix * 0.33f);
    rightChain.get<ChainPositions::Chorus2>().setMix(chainSettings.reverb2Mix * 0.33f);
    leftChain.get<ChainPositions::Chorus2>().setDepth(chainSettings.reverb1ModDepth);
    rightChain.get<ChainPositions::Chorus2>().setDepth(chainSettings.reverb1ModDepth);
    leftChain.get<ChainPositions::Chorus2>().setRate(chainSettings.reverb1ModRate - 0.001);
    rightChain.get<ChainPositions::Chorus2>().setRate(chainSettings.reverb1ModRate);

    leftChain.get<ChainPositions::Reverb1>().setParameters(reverb1Parameters);
    rightChain.get<ChainPositions::Reverb1>().setParameters(reverb1Parameters);

    leftChain.get<ChainPositions::Reverb2>().setParameters(reverb2Parameters);
    rightChain.get<ChainPositions::Reverb2>().setParameters(reverb2Parameters);

    auto lowpassCoefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(UniversalSampleRate, chainSettings.masterHighpass);
    auto highpassCoefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(UniversalSampleRate, chainSettings.masterLowpass);

    *leftChain.get<ChainPositions::LowPass>().coefficients = *lowpassCoefficients;
    *rightChain.get<ChainPositions::LowPass>().coefficients = *lowpassCoefficients;

    *leftChain.get<ChainPositions::HighPass>().coefficients = *highpassCoefficients;
    *rightChain.get<ChainPositions::HighPass>().coefficients = *highpassCoefficients;

    //linkChainSettings(chainSettings);
    
    juce::dsp::AudioBlock<float> block(buffer);
    auto leftBlock = block.getSingleChannelBlock(0);
    auto rightBlock = block.getSingleChannelBlock(1);

    juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
    juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);

    //leftChain.process(leftContext);
    //rightChain.process(rightContext);

    leftChain.get<ChainPositions::Reverb1>().process(leftContext);
    rightChain.get<ChainPositions::Reverb1>().process(rightContext);
    leftChain.get<ChainPositions::Reverb2>().process(leftContext);
    rightChain.get<ChainPositions::Reverb2>().process(rightContext);
    leftChain.get<ChainPositions::Chorus1>().process(leftContext);
    rightChain.get<ChainPositions::Chorus1>().process(rightContext);
    leftChain.get<ChainPositions::Chorus2>().process(leftContext);
    rightChain.get<ChainPositions::Chorus2>().process(rightContext);
    leftChain.get<ChainPositions::LowPass>().process(leftContext);
    rightChain.get<ChainPositions::LowPass>().process(rightContext);
    leftChain.get<ChainPositions::HighPass>().process(leftContext);
    rightChain.get<ChainPositions::HighPass>().process(rightContext);
}

//==============================================================================




//==============================================================================
//void MarsAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
//{
//    // You should use this method to store your parameters in the memory block.
//    // You could do that either as raw data, or use the XML or ValueTree classes
//    // as intermediaries to make it easy to save and load complex data.
//}
//
//void MarsAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
//{
//    // You should use this method to restore your parameters from this memory block,
//    // whose contents will have been created by the getStateInformation() call.
//}

//==============================================================================
ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts) {
    ChainSettings settings;

    //settings.dlTime = apvts.getRawParameterValue("dlTime")->load();
    //settings.dlFeedback = apvts.getRawParameterValue("dlFeedback")->load();

    settings.reverb1Amount = apvts.getRawParameterValue("reverb1Amount")->load();
    settings.reverb1Mix = apvts.getRawParameterValue("reverb1Mix")->load();
    settings.reverb1ModDepth = apvts.getRawParameterValue("reverb1ModDepth")->load();
    settings.reverb1ModRate = apvts.getRawParameterValue("reverb1ModRate")->load();

    settings.reverb2Amount = apvts.getRawParameterValue("reverb2Amount")->load();
    settings.reverb2Mix = apvts.getRawParameterValue("reverb2Mix")->load();
    settings.reverb2ModDepth = apvts.getRawParameterValue("reverb2ModDepth")->load();
    settings.reverb2ModRate = apvts.getRawParameterValue("reverb2ModRate")->load();

    settings.masterHighpass = apvts.getRawParameterValue("masterHighpass")->load();
    settings.masterLowpass = apvts.getRawParameterValue("masterLowpass")->load();
    //settings.masterDryWet = apvts.getRawParameterValue("dryWetMix")->load();

    return settings;
}


juce::AudioProcessorValueTreeState::ParameterLayout MarsAudioProcessor::createParameterLayout() {

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    //layout.add(std::make_unique<juce::AudioParameterFloat>(
    //    "dlTime", //parameterId
    //    "Delay Time", //parameter name
    //    juce::NormalisableRange<float>(
    //        0.1f, //minvalue
    //        5.0f, //max value
    //        0.1f, //interval
    //        0.5f//range with a skew factor
    //        ),
    //    2.f//default value
    //    )
    //);

    //layout.add(std::make_unique<juce::AudioParameterFloat>(
    //    "dlFeedback", //parameterId
    //    "Delay Feedback", //parameter name
    //    juce::NormalisableRange<float>(
    //        0.0f, //minvalue
    //        0.8f, //max value
    //        0.05f, //interval
    //        1.f//range with a skew factor
    //        ),
    //    0.5f//default value
    //    )
    //);

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "masterHighpass", //parameterId
        "Low Cut", //parameter name
        juce::NormalisableRange<float>(
            20.0f, //minvalue
            20000.0f, //max value
            1.f, //interval
            0.35f//range with a skew factor
            ),
        20000.f//default value
        )
    );

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "masterLowpass", //parameterId
        "High Cut", //parameter name
        juce::NormalisableRange<float>(
            20.0f, //minvalue
            20000.f, //max value
            1.f, //interval
            0.35f//range with a skew factor
            ),
        20.f//default value
        )
    );

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "reverb1Amount", //parameterId
        "Rev 1 Amount", //parameter name
        juce::NormalisableRange<float>(
            0.05f, //minvalue
            1.f, //max value
            0.05f, //interval
            1.f//range with a skew factor
            ),
        0.5f//default value
        )
    );

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "reverb1Mix", //parameterId
        "Rev 1 Mix", //parameter name
        juce::NormalisableRange<float>(
            0.0f, //minvalue
            1.0f, //max value
            0.05f, //interval
            1.f//range with a skew factor
            ),
        0.5f//default value
        )
    );

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "reverb1ModRate", //parameterId
        "Rev 1 Mod Rate", //parameter name
        juce::NormalisableRange<float>(
            0.002f, //minvalue
            10.f, //max value
            0.005f, //interval
            1.f//range with a skew factor
            ),
        0.5f//default value
        )
    );

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "reverb1ModDepth", //parameterId
        "Rev 1 ModDepth", //parameter name
        juce::NormalisableRange<float>(
            0.0f, //minvalue
            1.0f, //max value
            0.05f, //interval
            1.f//range with a skew factor
            ),
        0.5f//default value
        )
    );

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "reverb2Amount", //parameterId
        "Rev 2 Amount", //parameter name
        juce::NormalisableRange<float>(
            0.05f, //minvalue
            1.f, //max value
            0.05f, //interval
            1.f//range with a skew factor
            ),
        0.5f//default value
        )
    );

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "reverb2Mix", //parameterId
        "Rev 2 Mix", //parameter name
        juce::NormalisableRange<float>(
            0.0f, //minvalue
            1.0f, //max value
            0.05f, //interval
            1.f//range with a skew factor
            ),
        0.5f//default value
        )
    );

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "reverb2ModRate", //parameterId
        "Rev 2 Mod Rate", //parameter name
        juce::NormalisableRange<float>(
            0.002f, //minvalue
            10.f, //max value
            0.005f, //interval
            1.f//range with a skew factor
            ),
        0.5f//default value
        )
    );

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "reverb2ModDepth", //parameterId
        "Rev 2 ModDepth", //parameter name
        juce::NormalisableRange<float>(
            0.0f, //minvalue
            1.0f, //max value
            0.05f, //interval
            1.f//range with a skew factor
            ),
        0.5f//default value
        )
    );

    //layout.add(std::make_unique<juce::AudioParameterFloat>(
    //    "dryWetMix", //parameterId
    //    "Dry Wet Mix", //parameter name
    //    juce::NormalisableRange<float>(
    //        0.0f, //minvalue
    //        1.0f, //max value
    //        0.05f, //interval
    //        1.f//range with a skew factor
    //        ),
    //    0.5f//default value
    //    )
    //);

    /*juce::StringArray algoList;
    algoList.add("Hall");

    layout.add(std::make_unique <juce::AudioParameterChoice>(
        "Reverb 1",
        "Reverb 1",
        algoList,
        0
        )
    );*/

    return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MarsAudioProcessor();
}
