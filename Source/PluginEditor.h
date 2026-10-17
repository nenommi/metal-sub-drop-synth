#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class SubDropLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SubDropLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

// Pitch-over-time preview with a live playhead.
class DropDisplay : public juce::Component
{
public:
    explicit DropDisplay (SubDropProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;

private:
    SubDropProcessor& proc;
};

class SubDropEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SubDropEditor (SubDropProcessor&);
    ~SubDropEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void timerCallback() override;
    Knob& addKnob (const char* paramId, const juce::String& text, const juce::String& tooltip);
    void addCombo (juce::ComboBox&, std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>&,
                   const char* paramId);

    SubDropProcessor& proc;
    SubDropLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    DropDisplay display;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::map<juce::String, Knob*> knobById;

    juce::ComboBox presetBox, dropModeBox, dropDivBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> dropModeAtt, dropDivAtt;
    juce::ToggleButton syncButton { "Sync" };
    juce::ToggleButton gateButton { "Gated" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAtt, gateAtt;
    juce::TextButton auditionButton { "AUDITION" };
    bool auditionDown = false;

    juce::Rectangle<int> pitchArea, ampArea, mixArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubDropEditor)
};
