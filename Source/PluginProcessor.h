#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include "PitchEngine.h"

class RelapseTuneAudioProcessor : public juce::AudioProcessor
{
public:
    RelapseTuneAudioProcessor();
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "RelapseTune"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    int getMask() const;                 // allowed pitch classes (bit 0 = C)
    void setCustomMask (int mask);       // switches scale to "Custom"

    juce::AudioProcessorValueTreeState apvts;
    std::array<PitchEngine, 2> engines;

    static const juce::StringArray& keyNames();
    static const juce::StringArray& scaleNames();

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    std::vector<float> dl; int dlW = 0; float phA = 0.f, phB = 0.f; double sampleRate = 44100.0;
    float readDelay (float d) const;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RelapseTuneAudioProcessor)
};
