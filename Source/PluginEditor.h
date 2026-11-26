#pragma once
#include "PluginProcessor.h"

class RelapseTuneEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RelapseTuneEditor (RelapseTuneAudioProcessor& p) : AudioProcessorEditor (&p), proc (p)
    {
        keyBox.addItemList (RelapseTuneAudioProcessor::keyNames(), 1);
        scaleBox.addItemList (RelapseTuneAudioProcessor::scaleNames(), 1);
        addAndMakeVisible (keyBox); addAndMakeVisible (scaleBox);
        for (auto* s : { &speed, &amount, &refA })
        {
            s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
            s->setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffff3b6b));
            addAndMakeVisible (*s);
        }
        speed.setTextValueSuffix (" ms"); refA.setTextValueSuffix (" Hz");
        amount.setTextValueSuffix (" %");
        amount.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100)); };
        keyA   = std::make_unique<CA> (proc.apvts, "key", keyBox);
        scaleA = std::make_unique<CA> (proc.apvts, "scale", scaleBox);
        speedA = std::make_unique<SA> (proc.apvts, "speed", speed);
        amountA = std::make_unique<SA> (proc.apvts, "amount", amount);
        refA_  = std::make_unique<SA> (proc.apvts, "refa", refA);
        setSize (480, 360);
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff14101a));
        g.setColour (juce::Colour (0xffff3b6b));
        g.setFont (juce::FontOptions (28.f, juce::Font::bold));
        g.drawText ("RELAPSETUNE", 20, 10, 300, 36, juce::Justification::centredLeft);
        g.setColour (juce::Colours::grey); g.setFont (12.f);
        g.drawText ("KEY", 20, 56, 100, 16, juce::Justification::left);
        g.drawText ("SCALE", 140, 56, 100, 16, juce::Justification::left);
        g.drawText ("RETUNE SPEED", 20, 150, 130, 16, juce::Justification::centred);
        g.drawText ("AMOUNT", 175, 150, 130, 16, juce::Justification::centred);
        g.drawText ("REF A", 330, 150, 130, 16, juce::Justification::centred);

        // pitch meter
        auto m = meterArea.toFloat();
        g.setColour (juce::Colour (0xff1f1a2a)); g.fillRoundedRectangle (m, 8.f);
        const float det = proc.engines[0].dbgMidi, tgt = proc.engines[0].dbgTarget;
        if (det > 0.f && tgt > 0.f)
        {
            const float cents = juce::jlimit (-50.f, 50.f, (det - tgt) * 100.f);
            const float cx = m.getCentreX();
            g.setColour (juce::Colours::white.withAlpha (0.3f));
            g.drawLine (cx, m.getY() + 6, cx, m.getBottom() - 6, 1.f);
            g.setColour (std::abs (cents) < 10.f ? juce::Colour (0xff3bff9a) : juce::Colour (0xffff3b6b));
            g.fillEllipse (cx + cents / 50.f * (m.getWidth() / 2 - 14) - 7, m.getCentreY() - 7, 14.f, 14.f);
            g.setColour (juce::Colours::white); g.setFont (18.f);
            g.drawText (noteName (tgt), m.removeFromTop (26.f), juce::Justification::centred);
        }
        else { g.setColour (juce::Colours::grey); g.setFont (14.f); g.drawText ("no pitch", m, juce::Justification::centred); }
    }

    void resized() override
    {
        keyBox.setBounds (20, 74, 100, 26); scaleBox.setBounds (140, 74, 160, 26);
        speed.setBounds (20, 170, 130, 120); amount.setBounds (175, 170, 130, 120); refA.setBounds (330, 170, 130, 120);
        meterArea = { 20, 302, getWidth() - 40, 46 };
    }

private:
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    static juce::String noteName (float midi)
    {
        const int m = juce::roundToInt (midi);
        return RelapseTuneAudioProcessor::keyNames()[((m % 12) + 12) % 12] + juce::String (m / 12 - 1);
    }
    void timerCallback() override { repaint (meterArea); }

    RelapseTuneAudioProcessor& proc;
    juce::ComboBox keyBox, scaleBox;
    juce::Slider speed, amount, refA;
    juce::Rectangle<int> meterArea;
    std::unique_ptr<CA> keyA, scaleA;
    std::unique_ptr<SA> speedA, amountA, refA_;
};
