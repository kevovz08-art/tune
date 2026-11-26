#pragma once
#include "PluginProcessor.h"

static const juce::Colour kPurple (0xff9b7bff), kPink (0xffe86a9a), kBg (0xff0c0b11), kPanel (0xff17151d);

// Round knobs with the value printed inside, MetaTune-style
struct MetaLNF : juce::LookAndFeel_V4
{
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.f);
        const float d = juce::jmin (b.getWidth(), b.getHeight());
        juce::Rectangle<float> r (d, d); r.setCentre (b.getCentre());
        const auto c = r.getCentre(); const float rad = d * 0.5f - 6.f;
        g.setColour (kPanel); g.fillEllipse (r);
        juce::Path track; track.addCentredArc (c.x, c.y, rad, rad, 0.f, a0, a1, true);
        g.setColour (juce::Colour (0xff2b2833)); g.strokePath (track, juce::PathStrokeType (8.f));
        juce::Path val; val.addCentredArc (c.x, c.y, rad, rad, 0.f, a0, a0 + pos * (a1 - a0), true);
        g.setGradientFill (juce::ColourGradient (kPink, r.getX(), r.getBottom(), kPurple, r.getRight(), r.getY(), false));
        g.strokePath (val, juce::PathStrokeType (8.f));
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (d * 0.17f, juce::Font::bold));
        g.drawText (s.getTextFromValue (s.getValue()), r.withSizeKeepingCentre (d * 0.8f, d * 0.25f).translated (0, -d * 0.06f), juce::Justification::centred);
        g.setColour (juce::Colours::grey); g.setFont (d * 0.1f);
        g.drawText (s.getName(), r.withSizeKeepingCentre (d * 0.8f, d * 0.2f).translated (0, d * 0.15f), juce::Justification::centred);
    }
};

// Animated orb: ring turns pink when the singer is off-pitch, shows the note being sung
class OrbComponent : public juce::Component
{
public:
    explicit OrbComponent (RelapseTuneAudioProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat(); auto c = b.getCentre();
        const float R = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f - 4.f;
        const float det = proc.engines[0].dbgMidi, tgt = proc.engines[0].dbgTarget;
        const bool active = det > 0.f && tgt > 0.f;
        const float err = active ? juce::jmin (1.f, std::abs (det - tgt) * 2.f) : 0.f;
        const float t = (float) juce::Time::getMillisecondCounter() * 0.001f;

        g.setGradientFill (juce::ColourGradient (juce::Colour (0x00ffffff), c.x, c.y, juce::Colour (0x55ffffff), c.x + R, c.y, true));
        g.fillEllipse (c.x - R, c.y - R, 2 * R, 2 * R);
        g.setColour (kBg); g.fillEllipse (c.x - R * 0.82f, c.y - R * 0.82f, R * 1.64f, R * 1.64f);

        g.setColour (kPurple.interpolatedWith (kPink, err).withAlpha (active ? 0.9f : 0.35f));
        g.drawEllipse (c.x - R * 0.7f, c.y - R * 0.7f, R * 1.4f, R * 1.4f, 5.f);

        const float core = R * 0.42f * (1.f + (active ? 0.06f * std::sin (t * 5.f) : 0.f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3bd6c6), c.x - core, c.y - core,
                                                 kPink, c.x + core, c.y + core, false));
        g.setOpacity (active ? 0.9f : 0.35f);
        g.fillEllipse (c.x - core, c.y - core, 2 * core, 2 * core);
        g.setOpacity (1.f);

        g.setColour (juce::Colours::white); g.setFont (juce::FontOptions (R * 0.28f, juce::Font::bold));
        g.drawText (active ? noteName (tgt) : "--", getLocalBounds(), juce::Justification::centred);
    }
    static juce::String noteName (float midi)
    {
        const int m = juce::roundToInt (midi);
        return RelapseTuneAudioProcessor::keyNames()[((m % 12) + 12) % 12];
    }
private:
    RelapseTuneAudioProcessor& proc;
};

// One-octave piano: click a key to turn that note on/off (switches scale to "Custom")
class KeyboardComponent : public juce::Component
{
public:
    explicit KeyboardComponent (RelapseTuneAudioProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override
    {
        const int mask = proc.getMask();
        const float tgt = proc.engines[0].dbgTarget;
        const int tpc = tgt > 0.f ? ((juce::roundToInt (tgt) % 12) + 12) % 12 : -1;
        for (int pass = 0; pass < 2; ++pass)
            for (int pc = 0; pc < 12; ++pc)
            {
                if (isBlack (pc) != (pass == 1)) continue;
                const bool on = (mask >> pc) & 1;
                auto r = rectFor (pc);
                g.setColour (isBlack (pc) ? (on ? juce::Colour (0xff5a46b0) : juce::Colour (0xff1a1a20))
                                          : (on ? kPurple : juce::Colour (0xffbdbdc6)));
                g.fillRoundedRectangle (r, 4.f);
                if (pc == tpc) { g.setColour (juce::Colours::white); g.drawRoundedRectangle (r.reduced (1.f), 4.f, 2.f); }
                if (! isBlack (pc))
                {
                    g.setColour (on ? juce::Colours::white : juce::Colours::black.withAlpha (0.6f)); g.setFont (13.f);
                    g.drawText (RelapseTuneAudioProcessor::keyNames()[pc], r.withTrimmedBottom (4.f), juce::Justification::centredBottom);
                }
            }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int pass = 0; pass < 2; ++pass)
            for (int pc = 0; pc < 12; ++pc)
                if (isBlack (pc) == (pass == 0) && rectFor (pc).contains (e.position))
                { proc.setCustomMask (proc.getMask() ^ (1 << pc)); repaint(); return; }
    }
private:
    static bool isBlack (int pc) { return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }
    static int whiteIdx (int pc) { switch (pc) { case 0: return 0; case 2: return 1; case 4: return 2; case 5: return 3; case 7: return 4; case 9: return 5; default: return 6; } }
    juce::Rectangle<float> rectFor (int pc) const
    {
        const float ww = (float) getWidth() / 7.f, h = (float) getHeight();
        if (! isBlack (pc)) return { (float) whiteIdx (pc) * ww + 1.f, 0.f, ww - 2.f, h };
        const int after = pc == 1 ? 0 : pc == 3 ? 1 : pc == 6 ? 3 : pc == 8 ? 4 : 5;
        const float bw = ww * 0.6f;
        return { (float) (after + 1) * ww - bw / 2.f, 0.f, bw, h * 0.62f };
    }
    RelapseTuneAudioProcessor& proc;
};

class RelapseTuneEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RelapseTuneEditor (RelapseTuneAudioProcessor& p)
        : AudioProcessorEditor (&p), proc (p), orb (p), keyboard (p)
    {
        auto setup = [this] (juce::Slider& s, const juce::String& id, const juce::String& name)
        {
            s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            s.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
            s.setName (name); s.setLookAndFeel (&lnf); addAndMakeVisible (s);
            sAtt.push_back (std::make_unique<SA> (proc.apvts, id, s));
        };
        setup (speed, "speed", "Speed");   speed.setNumDecimalPlacesToDisplay (1);
        setup (amount, "amount", "Amount");
        setup (doubler, "doubler", "Doubler");
        setup (formant, "formant", "Formant");
        setup (detune, "refa", "Detune");  detune.setTextValueSuffix (" Hz"); detune.setNumDecimalPlacesToDisplay (1);
        for (auto* s : { &amount, &doubler })
            s->textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100)); };
        formant.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v)); };

        transpose.setSliderStyle (juce::Slider::IncDecButtons);
        transpose.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 44, 26);
        transpose.setIncDecButtonsMode (juce::Slider::incDecButtonsNotDraggable);
        addAndMakeVisible (transpose);
        sAtt.push_back (std::make_unique<SA> (proc.apvts, "transpose", transpose));

        addAndMakeVisible (preserve);
        preserveA = std::make_unique<BA> (proc.apvts, "formantOn", preserve);

        keyBox.addItemList (RelapseTuneAudioProcessor::keyNames(), 1);
        scaleBox.addItemList (RelapseTuneAudioProcessor::scaleNames(), 1);
        addAndMakeVisible (keyBox); addAndMakeVisible (scaleBox);
        keyA = std::make_unique<CA> (proc.apvts, "key", keyBox);
        scaleA = std::make_unique<CA> (proc.apvts, "scale", scaleBox);

        const char* stabNames[4] = { "None", "Short", "Mid", "Long" };
        for (int i = 0; i < 4; ++i)
        {
            auto& b = stabBtn[i];
            b.setButtonText (stabNames[i]); b.setClickingTogglesState (true); b.setRadioGroupId (101);
            b.setColour (juce::TextButton::buttonColourId, kPanel);
            b.setColour (juce::TextButton::buttonOnColourId, kPurple);
            b.onClick = [this, i]
            {
                if (auto* prm = proc.apvts.getParameter ("stab"))
                { prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->convertTo0to1 ((float) i)); prm->endChangeGesture(); }
            };
            addAndMakeVisible (b);
        }
        addAndMakeVisible (orb); addAndMakeVisible (keyboard);
        setSize (860, 730);
        startTimerHz (30);
    }

    ~RelapseTuneEditor() override { for (auto* s : { &speed, &amount, &doubler, &formant, &detune }) s->setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (kBg);
        g.setColour (juce::Colour (0xff131119)); g.fillRect (0, 0, getWidth(), 44);
        g.setFont (juce::FontOptions (24.f)); g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.drawText ("relapse", getWidth() / 2 - 100, 6, 100, 32, juce::Justification::centredRight);
        g.setFont (juce::FontOptions (24.f, juce::Font::bold)); g.setColour (kPurple);
        g.drawText ("Tune", getWidth() / 2, 6, 100, 32, juce::Justification::centredLeft);

        g.setColour (juce::Colours::grey); g.setFont (12.f);
        g.drawText ("SELECT", 20, 452, 60, 28, juce::Justification::centredLeft);
        g.drawText ("NOTE STABILIZER", 420, 452, 130, 28, juce::Justification::centredRight);
        g.drawText ("TRANSPOSE", 400, 640, 130, 16, juce::Justification::centred);

        // key / scale readout panel
        g.setColour (kPanel); g.fillRect (10, 495, 115, 125);
        const int key = (int) *proc.apvts.getRawParameterValue ("key");
        const int sc = (int) *proc.apvts.getRawParameterValue ("scale");
        const auto scName = RelapseTuneAudioProcessor::scaleNames()[sc];
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (sc == 7 ? 26.f : 44.f, juce::Font::bold));
        g.drawText (sc == 7 ? juce::String ("Custom") : RelapseTuneAudioProcessor::keyNames()[key], 10, 510, 115, 56, juce::Justification::centred);
        g.setColour (kPurple); g.setFont (14.f);
        g.drawText (sc == 7 ? "click keys" : scName, 10, 566, 115, 24, juce::Justification::centred);
    }

    void resized() override
    {
        orb.setBounds (30, 52, 450, 385);
        speed.setBounds (500, 45, 200, 200);
        doubler.setBounds (712, 105, 110, 110);
        amount.setBounds (600, 250, 190, 190);
        keyBox.setBounds (90, 452, 80, 28); scaleBox.setBounds (180, 452, 150, 28);
        for (int i = 0; i < 4; ++i) stabBtn[i].setBounds (560 + i * 62, 452, 60, 28);
        keyboard.setBounds (130, 495, 720, 125);
        formant.setBounds (130, 630, 100, 90); preserve.setBounds (235, 662, 110, 24);
        transpose.setBounds (410, 664, 110, 28);
        detune.setBounds (640, 630, 100, 90);
    }

private:
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override
    {
        const int idx = juce::jlimit (0, 3, (int) *proc.apvts.getRawParameterValue ("stab"));
        for (int i = 0; i < 4; ++i) stabBtn[i].setToggleState (i == idx, juce::dontSendNotification);
        orb.repaint(); keyboard.repaint(); repaint (10, 495, 115, 125);
    }

    RelapseTuneAudioProcessor& proc;
    MetaLNF lnf;
    OrbComponent orb;
    KeyboardComponent keyboard;
    juce::Slider speed, amount, doubler, formant, detune, transpose;
    juce::ToggleButton preserve { "Preserve" };
    juce::ComboBox keyBox, scaleBox;
    juce::TextButton stabBtn[4];
    std::unique_ptr<CA> keyA, scaleA;
    std::unique_ptr<BA> preserveA;
    std::vector<std::unique_ptr<SA>> sAtt;
};
