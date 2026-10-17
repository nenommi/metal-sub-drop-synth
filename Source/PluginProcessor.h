#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "SubDropEngine.h"

namespace ParamIDs
{
    inline constexpr auto tune       = "tune";
    inline constexpr auto fine       = "fine";
    inline constexpr auto dropMode   = "dropMode";
    inline constexpr auto dropAmount = "dropAmount";
    inline constexpr auto dropTime   = "dropTime";
    inline constexpr auto dropSync   = "dropSync";
    inline constexpr auto dropDiv    = "dropDiv";
    inline constexpr auto dropCurve  = "dropCurve";
    inline constexpr auto floorHz    = "floor";
    inline constexpr auto punch      = "punch";
    inline constexpr auto attack     = "attack";
    inline constexpr auto hold       = "hold";
    inline constexpr auto decay      = "decay";
    inline constexpr auto release    = "release";
    inline constexpr auto trigMode   = "trigMode";
    inline constexpr auto velSens    = "velSens";
    inline constexpr auto drive      = "drive";
    inline constexpr auto hpf        = "hpf";
    inline constexpr auto lpf        = "lpf";
    inline constexpr auto ceiling    = "ceiling";
    inline constexpr auto gain       = "gain";
}

class SubDropProcessor : public juce::AudioProcessor
{
public:
    SubDropProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    // Factory presets are offered in the editor only. Exposing them as host programs would let a
    // host reset every parameter by restoring the VST3 program-change parameter.
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Current parameter values as engine params (also used by the editor's curve display).
    EngineParams buildEngineParams() const;

    static int getNumPresets();
    static juce::String getPresetName (int index);
    void loadPreset (int index);

    void auditionStart() { auditionOn.store (true); }
    void auditionStop()  { auditionOff.store (true); }

    juce::AudioProcessorValueTreeState apvts;
    SubDropEngine engine;

    static juce::StringArray dropDivisionNames();

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float param (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    std::atomic<double> hostBpm { 120.0 };
    std::atomic<bool> auditionOn { false }, auditionOff { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubDropProcessor)
};
