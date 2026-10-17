#include "PluginEditor.h"

namespace Colours
{
    const juce::Colour background { 0xff111114 };
    const juce::Colour panel      { 0xff1b1b20 };
    const juce::Colour outline    { 0xff2c2c34 };
    const juce::Colour text       { 0xffd8d8de };
    const juce::Colour dimText    { 0xff80808c };
    const juce::Colour accent     { 0xffff4a26 };
    const juce::Colour accentDim  { 0xff5a2216 };
}

//==============================================================================
SubDropLookAndFeel::SubDropLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, Colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, Colours::text);
    setColour (juce::ComboBox::backgroundColourId, Colours::background);
    setColour (juce::ComboBox::outlineColourId, Colours::outline);
    setColour (juce::ComboBox::textColourId, Colours::text);
    setColour (juce::ComboBox::arrowColourId, Colours::accent);
    setColour (juce::PopupMenu::backgroundColourId, Colours::panel);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::accentDim);
    setColour (juce::ToggleButton::textColourId, Colours::text);
    setColour (juce::ToggleButton::tickColourId, Colours::accent);
    setColour (juce::ToggleButton::tickDisabledColourId, Colours::dimText);
    setColour (juce::TextButton::buttonColourId, Colours::accentDim);
    setColour (juce::TextButton::buttonOnColourId, Colours::accent);
    setColour (juce::TextButton::textColourOffId, Colours::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::TooltipWindow::backgroundColourId, Colours::panel);
    setColour (juce::TooltipWindow::textColourId, Colours::text);
    setColour (juce::TooltipWindow::outlineColourId, Colours::outline);
}

void SubDropLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float trackWidth = 3.5f;
    const float arcRadius = radius - trackWidth;
    const bool enabled = slider.isEnabled();

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (Colours::outline);
    g.strokePath (track, juce::PathStrokeType (trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Bipolar ranges draw their arc from the centre.
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float zeroPos = bipolar ? (float) slider.valueToProportionOfLength (0.0) : 0.0f;
    const float zeroAngle = startAngle + zeroPos * (endAngle - startAngle);

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                         juce::jmin (zeroAngle, angle), juce::jmax (zeroAngle, angle), true);
    g.setColour (enabled ? Colours::accent : Colours::dimText);
    g.strokePath (value, juce::PathStrokeType (trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float knobRadius = arcRadius - 6.0f;
    g.setColour (Colours::panel.brighter (0.15f));
    g.fillEllipse (centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -knobRadius + 2.0f, 3.0f, knobRadius * 0.5f, 1.5f);
    g.setColour (enabled ? Colours::text : Colours::dimText);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
}

//==============================================================================
void DropDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (Colours::background);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (Colours::outline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

    const auto p = proc.buildEngineParams();
    const int note = proc.engine.uiNote.load();
    const auto plot = bounds.reduced (12.0f, 10.0f).withTrimmedLeft (34.0f).withTrimmedBottom (14.0f);

    const float duration = juce::jlimit (0.3f, 15.0f, juce::jmax (p.dropTimeSec * 1.25f,
                                                                  p.attackSec + p.holdSec + p.decaySec * 0.6f));
    constexpr int steps = 300;
    float maxHz = 0.0f;
    for (int i = 0; i <= steps; ++i)
        maxHz = juce::jmax (maxHz, SubDropEngine::pitchAt (duration * (float) i / steps, note, p));

    const float fMin = 10.0f;
    const float fMax = juce::jlimit (100.0f, 4000.0f, maxHz * 1.4f);
    auto yFor = [&] (float hz)
    {
        const float norm = std::log (juce::jmax (hz, fMin) / fMin) / std::log (fMax / fMin);
        return plot.getBottom() - juce::jlimit (0.0f, 1.0f, norm) * plot.getHeight();
    };
    auto xFor = [&] (float t) { return plot.getX() + (t / duration) * plot.getWidth(); };

    // Region below the low cut: anything the drop puts here is being filtered away.
    const float hpfY = yFor (p.hpfHz);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (plot.getX(), hpfY, plot.getRight(), plot.getBottom()));

    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    for (float hz : { 20.0f, 30.0f, 40.0f, 60.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f })
    {
        if (hz >= fMax) break;
        const float y = yFor (hz);
        g.setColour (Colours::outline.withAlpha (0.6f));
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        g.setColour (Colours::dimText);
        g.drawText (hz >= 1000.0f ? juce::String (hz / 1000.0f, 0) + "k" : juce::String ((int) hz),
                    juce::Rectangle<float> (bounds.getX() + 4.0f, y - 6.0f, 36.0f, 12.0f), juce::Justification::centredRight);
    }

    g.setColour (Colours::dimText);
    g.drawText ("0 s", juce::Rectangle<float> (plot.getX(), plot.getBottom() + 2.0f, 40.0f, 12.0f), juce::Justification::left);
    g.drawText (juce::String (duration, 2) + " s", juce::Rectangle<float> (plot.getRight() - 60.0f, plot.getBottom() + 2.0f, 60.0f, 12.0f),
                juce::Justification::right);

    // Amplitude envelope (one-shot shape) as a faint fill.
    juce::Path amp;
    amp.startNewSubPath (plot.getX(), plot.getBottom());
    for (int i = 0; i <= steps; ++i)
    {
        const float t = duration * (float) i / steps;
        float level;
        if (t < p.attackSec)                     level = t / juce::jmax (p.attackSec, 1.0e-4f);
        else if (t < p.attackSec + p.holdSec)    level = 1.0f;
        else level = std::pow (1.0e-4f, (t - p.attackSec - p.holdSec) / juce::jmax (p.decaySec, 1.0e-3f));
        amp.lineTo (xFor (t), plot.getBottom() - level * plot.getHeight() * 0.35f);
    }
    amp.lineTo (plot.getRight(), plot.getBottom());
    amp.closeSubPath();
    g.setColour (Colours::accent.withAlpha (0.10f));
    g.fillPath (amp);

    // Floor marker.
    {
        const float y = yFor (p.floorHz);
        juce::Path floorLine;
        floorLine.startNewSubPath (plot.getX(), y);
        floorLine.lineTo (plot.getRight(), y);
        juce::Path dashed;
        const float dashes[] = { 4.0f, 4.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, floorLine, dashes, 2);
        g.setColour (Colours::dimText);
        g.fillPath (dashed);
    }

    // Pitch curve.
    juce::Path curve;
    for (int i = 0; i <= steps; ++i)
    {
        const float t = duration * (float) i / steps;
        const auto pt = juce::Point<float> (xFor (t), yFor (SubDropEngine::pitchAt (t, note, p)));
        if (i == 0) curve.startNewSubPath (pt); else curve.lineTo (pt);
    }
    g.setColour (Colours::accent);
    g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (proc.engine.active.load())
    {
        const float t = proc.engine.uiElapsed.load();
        if (t <= duration)
        {
            const float level = proc.engine.uiLevel.load();
            const juce::Point<float> pt (xFor (t), yFor (SubDropEngine::pitchAt (t, note, p)));
            g.setColour (juce::Colours::white.withAlpha (0.25f + 0.75f * level));
            g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (pt));
        }
    }

    // Start -> end readout so the boom can be matched to the song's key.
    auto noPunch = p;
    noPunch.punch = 0.0f;
    const float startHz = SubDropEngine::pitchAt (0.0f, note, noPunch);
    const float endHz = SubDropEngine::pitchAt (1.0e6f, note, p);
    auto noteName = [] (float hz)
    {
        const int n = juce::roundToInt (69.0f + 12.0f * std::log2 (hz / 440.0f));
        return juce::MidiMessage::getMidiNoteName (n, true, true, 4);
    };

    juce::String readout = noteName (startHz) + "  " + juce::String (startHz, 1) + " Hz   >   "
                         + noteName (endHz) + "  " + juce::String (endHz, 1) + " Hz";
    if (endHz <= p.floorHz + 0.01f)
        readout << "  (floor)";
    if (endHz < p.hpfHz)
        readout << "  - ends below Low Cut";

    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.setColour (Colours::text);
    g.drawText (readout, plot.toNearestInt().removeFromTop (16).withTrimmedLeft (8), juce::Justification::left);
}

//==============================================================================
SubDropEditor::SubDropEditor (SubDropProcessor& p)
    : AudioProcessorEditor (p), proc (p), display (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (display);

    addKnob (ParamIDs::tune,       "TUNE",     "Transpose the whole boom in semitones (match the song key).");
    addKnob (ParamIDs::fine,       "FINE",     "Fine tune in cents.");
    addKnob (ParamIDs::dropAmount, "DROP",     "How far the pitch travels, in semitones (12 = one octave).");
    addKnob (ParamIDs::dropTime,   "TIME",     "How long the drop takes.");
    addKnob (ParamIDs::dropCurve,  "CURVE",    "Positive = falls fast then settles (punchy). Negative = holds then dives.");
    addKnob (ParamIDs::floorHz,    "FLOOR",    "The pitch never goes below this. Keeps the tail audible instead of turning into rumble.");
    addKnob (ParamIDs::punch,      "PUNCH",    "Short pitch blip at the start so the hit cuts through guitars.");

    addKnob (ParamIDs::attack,     "ATTACK",   "Fade-in time. A couple of ms avoids clicks.");
    addKnob (ParamIDs::hold,       "HOLD",     "Time at full level before the decay starts.");
    addKnob (ParamIDs::decay,      "DECAY",    "Fade-out length after the hold.");
    addKnob (ParamIDs::release,    "RELEASE",  "Fade-out after note-off (Gated mode only).");
    addKnob (ParamIDs::velSens,    "VELOCITY", "How much MIDI velocity affects level.");

    addKnob (ParamIDs::drive,      "DRIVE",    "Saturation. Adds harmonics so the sub is heard on small speakers.");
    addKnob (ParamIDs::hpf,        "LOW CUT",  "24 dB/oct high-pass. Removes headroom-eating infrasonics.");
    addKnob (ParamIDs::lpf,        "HIGH CUT", "Low-pass. Tames drive harmonics so they don't clash with bass guitar and kick.");
    addKnob (ParamIDs::ceiling,    "CEILING",  "Soft-clip ceiling on the output. Stops the boom from overshooting.");
    addKnob (ParamIDs::gain,       "OUTPUT",   "Output level.");

    addCombo (dropModeBox, dropModeAtt, ParamIDs::dropMode);
    addCombo (dropDivBox, dropDivAtt, ParamIDs::dropDiv);
    dropModeBox.setTooltip ("Fall: starts on the played note and drops.\nLand: starts above and lands on the played note (ends in key).");
    gateAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ParamIDs::trigMode, gateButton);
    gateButton.setTooltip ("Off (One-Shot): every hit plays the whole envelope.\nOn (Gated): note-off starts the release, so MIDI note length sets the boom length.");
    addAndMakeVisible (gateButton);
    dropDivBox.setTooltip ("Drop length in host tempo when Sync is on.");

    syncAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ParamIDs::dropSync, syncButton);
    syncButton.setTooltip ("Lock the drop time to the host tempo.");
    addAndMakeVisible (syncButton);

    for (int i = 0; i < SubDropProcessor::getNumPresets(); ++i)
        presetBox.addItem (SubDropProcessor::getPresetName (i), i + 1);
    presetBox.setTextWhenNothingSelected ("Presets");
    presetBox.onChange = [this]
    {
        if (presetBox.getSelectedId() > 0)
            proc.loadPreset (presetBox.getSelectedId() - 1);
    };
    addAndMakeVisible (presetBox);

    auditionButton.setTooltip ("Play the last received note (E1 by default). Hold to sustain in Gated mode.");
    auditionButton.onStateChange = [this]
    {
        const bool down = auditionButton.isDown();
        if (down != auditionDown)
        {
            down ? proc.auditionStart() : proc.auditionStop();
            auditionDown = down;
        }
    };
    addAndMakeVisible (auditionButton);

    setSize (980, 580);
    startTimerHz (30);
}

SubDropEditor::~SubDropEditor()
{
    setLookAndFeel (nullptr);
}

SubDropEditor::Knob& SubDropEditor::addKnob (const char* paramId, const juce::String& text, const juce::String& tooltip)
{
    auto knob = std::make_unique<Knob>();
    knob->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    knob->slider.setTooltip (tooltip);
    knob->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, knob->slider);
    knob->label.setText (text, juce::dontSendNotification);
    knob->label.setJustificationType (juce::Justification::centred);
    knob->label.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    knob->label.setColour (juce::Label::textColourId, Colours::dimText);
    addAndMakeVisible (knob->slider);
    addAndMakeVisible (knob->label);

    auto& ref = *knob;
    knobById[paramId] = knob.get();
    knobs.push_back (std::move (knob));
    return ref;
}

void SubDropEditor::addCombo (juce::ComboBox& box, std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>& att,
                              const char* paramId)
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (paramId)))
        box.addItemList (choice->choices, 1);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, paramId, box);
    addAndMakeVisible (box);
}

void SubDropEditor::timerCallback()
{
    const bool synced = syncButton.getToggleState();
    knobById[ParamIDs::dropTime]->slider.setEnabled (! synced);
    dropDivBox.setEnabled (synced);
    knobById[ParamIDs::release]->slider.setEnabled (gateButton.getToggleState());
    display.repaint();
}

void SubDropEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::background);

    auto header = getLocalBounds().removeFromTop (48).reduced (16, 0);
    g.setColour (Colours::accent);
    g.setFont (juce::Font (juce::FontOptions (24.0f, juce::Font::bold)));
    g.drawText ("SUB DROP", header, juce::Justification::centredLeft);
    g.setColour (Colours::dimText);
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText ("metal boom synth", header.withTrimmedLeft (132), juce::Justification::centredLeft);

    auto drawPanel = [&] (juce::Rectangle<int> r, const juce::String& title)
    {
        g.setColour (Colours::panel);
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (Colours::outline);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 6.0f, 1.0f);
        g.setColour (Colours::accent);
        g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
        g.drawText (title, r.reduced (12, 6).removeFromTop (18), juce::Justification::left);
    };

    drawPanel (pitchArea, "PITCH DROP");
    drawPanel (ampArea, "AMP");
    drawPanel (mixArea, "MIX FIT");
}

void SubDropEditor::resized()
{
    auto area = getLocalBounds().reduced (16, 0);

    auto header = area.removeFromTop (48);
    auditionButton.setBounds (header.removeFromRight (110).reduced (0, 10));
    header.removeFromRight (10);
    presetBox.setBounds (header.removeFromRight (200).reduced (0, 12));

    display.setBounds (area.removeFromTop (180));
    area.removeFromTop (12);
    area.removeFromBottom (16);

    const int gap = 12;
    const int colW = (area.getWidth() - 2 * gap) / 10;
    pitchArea = area.removeFromLeft (colW * 4);
    area.removeFromLeft (gap);
    ampArea = area.removeFromLeft (colW * 3);
    area.removeFromLeft (gap);
    mixArea = area;

    auto layoutGrid = [] (juce::Rectangle<int> panel, int columns, std::initializer_list<juce::Component*> cells,
                          std::function<void (juce::Component*, juce::Rectangle<int>)> place)
    {
        auto inner = panel.reduced (8).withTrimmedTop (22);
        const int cellW = inner.getWidth() / columns;
        const int cellH = inner.getHeight() / 2;
        int i = 0;
        for (auto* c : cells)
        {
            if (c != nullptr)
                place (c, { inner.getX() + (i % columns) * cellW, inner.getY() + (i / columns) * cellH, cellW, cellH });
            ++i;
        }
    };

    auto placeKnob = [this] (juce::Component* c, juce::Rectangle<int> cell)
    {
        for (auto& k : knobs)
        {
            if (&k->slider != c) continue;
            k->label.setBounds (cell.removeFromTop (18));
            k->slider.setBounds (cell.reduced (4, 2));
        }
    };

    auto k = [this] (const char* id) -> juce::Component* { return &knobById[id]->slider; };

    layoutGrid (pitchArea, 4, { k (ParamIDs::tune), k (ParamIDs::fine), k (ParamIDs::dropAmount), k (ParamIDs::dropTime),
                                k (ParamIDs::dropCurve), k (ParamIDs::floorHz), k (ParamIDs::punch), nullptr }, placeKnob);
    layoutGrid (ampArea, 3, { k (ParamIDs::attack), k (ParamIDs::hold), k (ParamIDs::decay),
                              k (ParamIDs::release), k (ParamIDs::velSens), nullptr }, placeKnob);
    layoutGrid (mixArea, 3, { k (ParamIDs::drive), k (ParamIDs::hpf), k (ParamIDs::lpf),
                              k (ParamIDs::ceiling), k (ParamIDs::gain), nullptr }, placeKnob);

    // Option controls fill the empty last cell of the pitch and amp grids.
    auto lastCell = [] (juce::Rectangle<int> panel, int columns)
    {
        auto inner = panel.reduced (8).withTrimmedTop (22);
        const int cellW = inner.getWidth() / columns;
        const int cellH = inner.getHeight() / 2;
        return juce::Rectangle<int> (inner.getX() + (columns - 1) * cellW, inner.getY() + cellH, cellW, cellH).reduced (4, 10);
    };

    auto pitchOpts = lastCell (pitchArea, 4);
    dropModeBox.setBounds (pitchOpts.removeFromTop (26));
    pitchOpts.removeFromTop (10);
    syncButton.setBounds (pitchOpts.removeFromTop (24));
    pitchOpts.removeFromTop (6);
    dropDivBox.setBounds (pitchOpts.removeFromTop (26));

    auto ampOpts = lastCell (ampArea, 3);
    gateButton.setBounds (ampOpts.removeFromTop (24));
}
