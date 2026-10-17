#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr float kDivisionBeats[] = { 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f };

    struct Preset
    {
        const char* name;
        std::initializer_list<std::pair<const char*, float>> values; // plain (unnormalised) values
    };

    const Preset kPresets[] = {
        { "Init Drop", {} },
        { "Breakdown Boom",      { { ParamIDs::dropAmount, 24.0f }, { ParamIDs::dropTime, 3000.0f }, { ParamIDs::dropCurve, 0.4f },
                                   { ParamIDs::hold, 600.0f }, { ParamIDs::decay, 4500.0f }, { ParamIDs::drive, 15.0f } } },
        { "Tight Impact",        { { ParamIDs::dropAmount, 12.0f }, { ParamIDs::dropTime, 450.0f }, { ParamIDs::dropCurve, 0.7f },
                                   { ParamIDs::hold, 60.0f }, { ParamIDs::decay, 900.0f }, { ParamIDs::punch, 45.0f } } },
        { "One Bar Synced",      { { ParamIDs::dropSync, 1.0f }, { ParamIDs::dropDiv, 4.0f }, { ParamIDs::dropAmount, 19.0f },
                                   { ParamIDs::hold, 1500.0f }, { ParamIDs::decay, 3000.0f } } },
        { "Small Speaker Growl", { { ParamIDs::drive, 60.0f }, { ParamIDs::lpf, 1200.0f }, { ParamIDs::hpf, 35.0f },
                                   { ParamIDs::dropAmount, 12.0f }, { ParamIDs::punch, 25.0f } } },
        { "Rise Into Key",       { { ParamIDs::dropMode, 1.0f }, { ParamIDs::dropAmount, 12.0f }, { ParamIDs::dropTime, 800.0f },
                                   { ParamIDs::dropCurve, 0.6f }, { ParamIDs::hold, 1200.0f }, { ParamIDs::decay, 2000.0f } } },
    };
}

SubDropProcessor::SubDropProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

juce::StringArray SubDropProcessor::dropDivisionNames()
{
    return { "1/16", "1/8", "1/4", "1/2", "1 Bar", "2 Bars", "4 Bars" };
}

juce::AudioProcessorValueTreeState::ParameterLayout SubDropProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    // Text <-> value pairs. Unit switches (ms/s, Hz/kHz) happen on the rounded value so
    // value -> text -> value -> text round-trips identically, as hosts and validators expect.
    struct Format
    {
        std::function<String (float, int)> toText;
        std::function<float (const String&)> fromText;
    };

    auto number = [] (const String& t) { return t.getFloatValue(); };
    auto withUnit = [&] (int decimals, const char* unit) -> Format
    {
        return { [=] (float v, int) { return String (v, decimals) + unit; }, number };
    };

    const Format ms { [] (float v, int)
                      {
                          if (v >= 999.5f) return String (v / 1000.0f, 2) + " s";
                          if (v < 9.95f)   return String (v, 1) + " ms";
                          return String (roundToInt (v)) + " ms";
                      },
                      [] (const String& t)
                      {
                          const bool seconds = t.containsIgnoreCase ("s") && ! t.containsIgnoreCase ("ms");
                          return t.getFloatValue() * (seconds ? 1000.0f : 1.0f);
                      } };
    const Format hz { [] (float v, int)
                      {
                          return v >= 999.5f ? String (v / 1000.0f, 1) + " kHz" : String (roundToInt (v)) + " Hz";
                      },
                      [] (const String& t) { return t.getFloatValue() * (t.containsIgnoreCase ("k") ? 1000.0f : 1.0f); } };
    const Format curve { [] (float v, int)
                         {
                             const int r = roundToInt (v * 100.0f);
                             return String (r) + (r > 0 ? " fast" : r < 0 ? " slow" : "");
                         },
                         [] (const String& t) { return t.getFloatValue() * 0.01f; } };
    const Format pct { [] (float v, int) { return String (roundToInt (v)) + " %"; }, number };
    const Format db = withUnit (1, " dB");
    const Format st = withUnit (0, " st");
    const Format ct = withUnit (1, " ct");

    // Every range has a step so values snap to an exact grid; this keeps save -> load -> save
    // bit-identical despite float rounding in the skew conversion.
    auto range = [] (float lo, float hi, float step, float centre)
    {
        NormalisableRange<float> r (lo, hi, step);
        r.setSkewForCentre (centre);
        return r;
    };

    auto add = [&] (const char* id, const char* name, NormalisableRange<float> r, float def, const Format& format)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                                                           AudioParameterFloatAttributes()
                                                               .withStringFromValueFunction (format.toText)
                                                               .withValueFromStringFunction (format.fromText)));
    };

    // Pitch / drop
    add (ParamIDs::tune, "Tune", { -24.0f, 24.0f, 1.0f }, 0.0f, st);
    add (ParamIDs::fine, "Fine", { -100.0f, 100.0f, 0.1f }, 0.0f, ct);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::dropMode, 1 }, "Drop Mode",
                                                        StringArray { "Fall", "Land" }, 0));
    add (ParamIDs::dropAmount, "Drop Amount", { 0.0f, 48.0f, 1.0f }, 12.0f, st);
    add (ParamIDs::dropTime, "Drop Time", range (10.0f, 10000.0f, 0.1f, 800.0f), 1500.0f, ms);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::dropSync, 1 }, "Drop Sync",
                                                        StringArray { "Off", "On" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::dropDiv, 1 }, "Drop Division",
                                                        dropDivisionNames(), 4));
    add (ParamIDs::dropCurve, "Drop Curve", { -1.0f, 1.0f, 0.001f }, 0.3f, curve);
    add (ParamIDs::floorHz, "Floor", range (10.0f, 80.0f, 0.1f, 30.0f), 27.0f, hz);
    add (ParamIDs::punch, "Punch", { 0.0f, 100.0f, 0.1f }, 0.0f, pct);

    // Amp envelope
    add (ParamIDs::attack, "Attack", range (0.5f, 500.0f, 0.1f, 20.0f), 2.0f, ms);
    add (ParamIDs::hold, "Hold", range (0.0f, 10000.0f, 0.1f, 500.0f), 300.0f, ms);
    add (ParamIDs::decay, "Decay", range (20.0f, 15000.0f, 0.1f, 1500.0f), 2500.0f, ms);
    add (ParamIDs::release, "Release", range (5.0f, 5000.0f, 0.1f, 300.0f), 300.0f, ms);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::trigMode, 1 }, "Trigger Mode",
                                                        StringArray { "One-Shot", "Gated" }, 0));
    add (ParamIDs::velSens, "Velocity", { 0.0f, 100.0f, 0.1f }, 50.0f, pct);

    // Tone / mix fitting
    add (ParamIDs::drive, "Drive", { 0.0f, 100.0f, 0.1f }, 0.0f, pct);
    add (ParamIDs::hpf, "Low Cut", range (10.0f, 150.0f, 0.1f, 40.0f), 20.0f, hz);
    add (ParamIDs::lpf, "High Cut", range (80.0f, 20000.0f, 1.0f, 1500.0f), 4000.0f, hz);
    add (ParamIDs::ceiling, "Ceiling", { -24.0f, 0.0f, 0.1f }, -0.3f, db);
    add (ParamIDs::gain, "Output", { -36.0f, 12.0f, 0.1f }, -6.0f, db);

    return layout;
}

EngineParams SubDropProcessor::buildEngineParams() const
{
    EngineParams p;
    p.tuneSemis  = param (ParamIDs::tune);
    p.fineCents  = param (ParamIDs::fine);
    p.landOnNote = param (ParamIDs::dropMode) > 0.5f;
    p.dropSemis  = param (ParamIDs::dropAmount);
    p.dropCurve  = param (ParamIDs::dropCurve);
    p.floorHz    = param (ParamIDs::floorHz);
    p.punch      = param (ParamIDs::punch) * 0.01f;

    if (param (ParamIDs::dropSync) > 0.5f)
    {
        const int div = juce::jlimit (0, (int) std::size (kDivisionBeats) - 1, (int) param (ParamIDs::dropDiv));
        p.dropTimeSec = (float) (kDivisionBeats[div] * 60.0 / juce::jmax (20.0, hostBpm.load()));
    }
    else
    {
        p.dropTimeSec = param (ParamIDs::dropTime) * 0.001f;
    }

    p.attackSec  = param (ParamIDs::attack) * 0.001f;
    p.holdSec    = param (ParamIDs::hold) * 0.001f;
    p.decaySec   = param (ParamIDs::decay) * 0.001f;
    p.releaseSec = param (ParamIDs::release) * 0.001f;
    p.noteLength = param (ParamIDs::trigMode) > 0.5f;
    p.velSens    = param (ParamIDs::velSens) * 0.01f;

    p.drive      = param (ParamIDs::drive) * 0.01f;
    p.hpfHz      = param (ParamIDs::hpf);
    p.lpfHz      = param (ParamIDs::lpf);
    p.ceilingLin = juce::Decibels::decibelsToGain (param (ParamIDs::ceiling));
    p.gainLin    = juce::Decibels::decibelsToGain (param (ParamIDs::gain), -36.0f);
    return p;
}

void SubDropProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    engine.setParams (buildEngineParams());
}

bool SubDropProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void SubDropProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    if (auto* playHead = getPlayHead())
        if (auto position = playHead->getPosition())
            if (auto bpm = position->getBpm())
                hostBpm.store (*bpm);

    engine.setParams (buildEngineParams());

    if (auditionOn.exchange (false))
        engine.noteOn (engine.uiNote.load(), 1.0f);
    if (auditionOff.exchange (false))
        engine.noteOff (engine.uiNote.load());

    float* out = buffer.getWritePointer (0);
    int pos = 0;

    for (const auto metadata : midi)
    {
        const int eventPos = juce::jlimit (pos, buffer.getNumSamples(), metadata.samplePosition);
        engine.render (out + pos, eventPos - pos);
        pos = eventPos;

        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
            engine.noteOn (msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isNoteOff())
            engine.noteOff (msg.getNoteNumber());
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            engine.allNotesOff();
    }

    engine.render (out + pos, buffer.getNumSamples() - pos);

    for (int ch = 1; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom (ch, 0, buffer, 0, 0, buffer.getNumSamples());
}

int SubDropProcessor::getNumPresets()
{
    return (int) std::size (kPresets);
}

juce::String SubDropProcessor::getPresetName (int index)
{
    return juce::isPositiveAndBelow (index, getNumPresets()) ? kPresets[index].name : "";
}

void SubDropProcessor::loadPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPresets()))
        return;

    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    for (const auto& [id, value] : kPresets[index].values)
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (value));
}

void SubDropProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    // Values set by the host on the audio thread may not have been flushed into the
    // APVTS tree yet (that happens on a message-thread timer), so write them explicitly.
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (auto child = state.getChildWithProperty ("id", rp->paramID); child.isValid())
                child.setProperty ("value", rp->convertFrom0to1 (rp->getValue()), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SubDropProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            // Ask the host to re-read all parameter values (CLAP: params rescan, VST3: restartComponent).
            updateHostDisplay (ChangeDetails().withProgramChanged (true));
        }
}

juce::AudioProcessorEditor* SubDropProcessor::createEditor()
{
    return new SubDropEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SubDropProcessor();
}
