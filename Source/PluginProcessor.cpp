#include "PluginProcessor.h"
#include "PluginEditor.h"

const juce::StringArray& RelapseTuneAudioProcessor::keyNames()
{ static const juce::StringArray k { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" }; return k; }

const juce::StringArray& RelapseTuneAudioProcessor::scaleNames()
{ static const juce::StringArray s { "Chromatic","Major","Minor","Harmonic Minor","Pentatonic Major","Pentatonic Minor","Blues","Custom" }; return s; }

static const std::vector<std::vector<int>> kScales {
    { 0,1,2,3,4,5,6,7,8,9,10,11 }, { 0,2,4,5,7,9,11 }, { 0,2,3,5,7,8,10 }, { 0,2,3,5,7,8,11 },
    { 0,2,4,7,9 }, { 0,3,5,7,10 }, { 0,3,5,6,7,10 } };   // index 7 = Custom (uses n0..n11)

juce::AudioProcessorValueTreeState::ParameterLayout RelapseTuneAudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 }, "Key", keyNames(), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Scale", scaleNames(), 1));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "speed", 1 }, "Speed",
           NormalisableRange<float> (-100.f, 400.f, 0.1f), 20.f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "amount", 1 }, "Amount",
           NormalisableRange<float> (0.f, 1.f, 0.001f), 1.f));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "stab", 1 }, "Note Stabilizer",
           StringArray { "None", "Short", "Mid", "Long" }, 1));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "doubler", 1 }, "Doubler",
           NormalisableRange<float> (0.f, 1.f, 0.001f), 0.f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "formant", 1 }, "Formant",
           NormalisableRange<float> (0.f, 200.f, 1.f), 100.f));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "formantOn", 1 }, "Preserve Formants", true));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { "transpose", 1 }, "Transpose", -12, 12, 0));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "refa", 1 }, "Detune",
           NormalisableRange<float> (415.f, 466.f, 0.1f), 440.f));
    for (int i = 0; i < 12; ++i)
        l.add (std::make_unique<AudioParameterBool> (ParameterID { "n" + String (i), 1 }, "Note " + keyNames()[i], true));
    return l;
}

RelapseTuneAudioProcessor::RelapseTuneAudioProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout()) {}

int RelapseTuneAudioProcessor::getMask() const
{
    const int key = (int) *apvts.getRawParameterValue ("key");
    const int scale = (int) *apvts.getRawParameterValue ("scale");
    int mask = 0;
    if (scale >= (int) kScales.size())
    {
        for (int i = 0; i < 12; ++i)
            if (*apvts.getRawParameterValue ("n" + juce::String (i)) > 0.5f) mask |= 1 << i;
        if (mask == 0) mask = 0xFFF;
    }
    else
        for (int iv : kScales[(size_t) scale]) mask |= 1 << ((iv + key) % 12);
    return mask;
}

void RelapseTuneAudioProcessor::setCustomMask (int mask)
{
    for (int i = 0; i < 12; ++i)
        if (auto* p = apvts.getParameter ("n" + juce::String (i))) p->setValueNotifyingHost (((mask >> i) & 1) ? 1.f : 0.f);
    if (auto* p = apvts.getParameter ("scale")) p->setValueNotifyingHost (p->convertTo0to1 ((float) (scaleNames().size() - 1)));
}

void RelapseTuneAudioProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr; dl.assign ((size_t) (sr * 0.1), 0.f); dlW = 0;
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
    static const float stabMap[4] = { 0.f, 0.25f, 0.55f, 1.f };
    PitchEngine::Settings s;
    s.mask = getMask();
    s.speedMs = *apvts.getRawParameterValue ("speed");
    s.amount  = *apvts.getRawParameterValue ("amount");
    s.refA    = *apvts.getRawParameterValue ("refa");
    s.stabilizer = stabMap[juce::jlimit (0, 3, (int) *apvts.getRawParameterValue ("stab"))];
    s.transpose = *apvts.getRawParameterValue ("transpose");
    s.formantPhi = std::pow (2.f, (*apvts.getRawParameterValue ("formant") - 100.f) / 100.f * (8.f / 12.f));
    s.formantFollow = *apvts.getRawParameterValue ("formantOn") < 0.5f;
    const float dbl = *apvts.getRawParameterValue ("doubler");

    const int nCh = juce::jmin (buffer.getNumChannels(), 2);
    const int ns = buffer.getNumSamples();
    auto* L = buffer.getWritePointer (0);
    auto* R = nCh == 2 ? buffer.getWritePointer (1) : nullptr;
    const float twoPi = 6.2831853f;
    for (int i = 0; i < ns; ++i)
    {
        const float yl = engines[0].process (L[i], s);
        const float yr = R ? engines[1].process (R[i], s) : yl;
        if (dbl > 0.001f && ! dl.empty())
        {
            dl[(size_t) dlW] = 0.5f * (yl + yr);
            const float dA = (0.022f + 0.004f * std::sin (phA)) * (float) sampleRate;
            const float dB = (0.031f + 0.004f * std::sin (phB)) * (float) sampleRate;
            phA += twoPi * 0.35f / (float) sampleRate; if (phA > twoPi) phA -= twoPi;
            phB += twoPi * 0.27f / (float) sampleRate; if (phB > twoPi) phB -= twoPi;
            L[i] = yl + dbl * 0.7f * readDelay (dA);
            if (R) R[i] = yr + dbl * 0.7f * readDelay (dB);
            if (++dlW >= (int) dl.size()) dlW = 0;
        }
        else { L[i] = yl; if (R) R[i] = yr; }
    }
}

float RelapseTuneAudioProcessor::readDelay (float d) const
{
    const int size = (int) dl.size();
    float rp = (float) dlW - d;
    while (rp < 0.f) rp += (float) size;
    const int i0 = (int) rp % size, i1 = (i0 + 1) % size;
    const float f = rp - std::floor (rp);
    return dl[(size_t) i0] * (1.f - f) + dl[(size_t) i1] * f;
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
