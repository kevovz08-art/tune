#include "PluginProcessor.h"
#include "PluginEditor.h"

const juce::StringArray& RelapseTuneAudioProcessor::keyNames()
{ static const juce::StringArray k { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" }; return k; }

const juce::StringArray& RelapseTuneAudioProcessor::scaleNames()
{ static const juce::StringArray s { "Chromatic","Major","Minor","Harmonic Minor","Pentatonic Major","Pentatonic Minor","Blues" }; return s; }

static const std::vector<std::vector<int>> kScales {
    { 0,1,2,3,4,5,6,7,8,9,10,11 }, { 0,2,4,5,7,9,11 }, { 0,2,3,5,7,8,10 }, { 0,2,3,5,7,8,11 },
    { 0,2,4,7,9 }, { 0,3,5,7,10 }, { 0,3,5,6,7,10 } };

juce::AudioProcessorValueTreeState::ParameterLayout RelapseTuneAudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 }, "Key", keyNames(), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Scale", scaleNames(), 1));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "speed", 1 }, "Retune Speed",
           NormalisableRange<float> (0.f, 400.f, 0.1f, 0.5f), 20.f, AudioParameterFloatAttributes().withLabel ("ms")));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "amount", 1 }, "Amount",
           NormalisableRange<float> (0.f, 1.f, 0.001f), 1.f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "refa", 1 }, "Reference A",
           NormalisableRange<float> (415.f, 466.f, 0.1f), 440.f, AudioParameterFloatAttributes().withLabel ("Hz")));
    return l;
}

RelapseTuneAudioProcessor::RelapseTuneAudioProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout()) {}

void RelapseTuneAudioProcessor::prepareToPlay (double sr, int)
{
    for (auto& e : engines) e.prepare (sr);
    setLatencySamples (engines[0].latency());
}

bool RelapseTuneAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto o = l.getMainOutputChannelSet();
    return (o == juce::AudioChannelSet::mono() || o == juce::AudioChannelSet::stereo()) && o == l.getMainInputChannelSet();
}

void RelapseTuneAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    PitchEngine::Settings s;
    const int key = (int) *apvts.getRawParameterValue ("key");
    const int scale = juce::jlimit (0, (int) kScales.size() - 1, (int) *apvts.getRawParameterValue ("scale"));
    s.mask = 0;
    for (int iv : kScales[(size_t) scale]) s.mask |= 1 << ((iv + key) % 12);
    s.speedMs = *apvts.getRawParameterValue ("speed");
    s.amount  = *apvts.getRawParameterValue ("amount");
    s.refA    = *apvts.getRawParameterValue ("refa");

    const int nCh = juce::jmin (buffer.getNumChannels(), 2);
    for (int ch = 0; ch < nCh; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            d[i] = engines[(size_t) ch].process (d[i], s);
    }
}

juce::AudioProcessorEditor* RelapseTuneAudioProcessor::createEditor() { return new RelapseTuneEditor (*this); }

void RelapseTuneAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void RelapseTuneAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RelapseTuneAudioProcessor(); }
