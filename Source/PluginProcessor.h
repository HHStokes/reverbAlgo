/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
*/
class MarsAudioProcessor  : public foleys::MagicProcessor //: public juce::AudioProcessor
{
public:
    //==============================================================================
    MarsAudioProcessor();
    ~MarsAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    //void getStateInformation (juce::MemoryBlock& destData) override;
    //void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts{*this, nullptr, "Parameters", createParameterLayout()};

private:
    juce::dsp::Reverb::Parameters reverb1Parameters;
    juce::dsp::Reverb::Parameters reverb2Parameters;
    using Filter = juce::dsp::IIR::Filter<float>;
    using DelayLine = juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>;
    using SchroederChain = juce::dsp::ProcessorChain<DelayLine,Filter, Filter, Filter, Filter, Filter>;
    
    using Reverb = juce::dsp::Reverb;
    using Chorus = juce::dsp::Chorus<float>;
    using DryWet = juce::dsp::DryWetMixer<float>;
    using MonoChain = juce::dsp::ProcessorChain< Reverb, Chorus, Reverb, Chorus, Filter, Filter>;
    
    MonoChain leftChain, rightChain;

    juce::dsp::ProcessSpec spec;
    int samplesPerSecond = 2; 
    juce::AudioBuffer<float> dlAudioBuffer;
    juce::AudioBuffer<float> dryBuffer;
    int dlWritePosition{ 0 };
    float UniversalSampleRate{ 441000 };

    enum ChainPositions {
        Reverb1,
        Chorus1,
        Reverb2,
        Chorus2,
        LowPass,
        HighPass,
    };
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MarsAudioProcessor)
};
