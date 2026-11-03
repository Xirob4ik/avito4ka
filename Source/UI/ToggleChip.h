#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace clipline::ui
{
    /** On / off switch bound to a boolean parameter (LINK, CEILING). */
    class ToggleChip : public juce::Component,
                       public juce::SettableTooltipClient
    {
    public:
        ToggleChip (juce::RangedAudioParameter& parameter, juce::String label, juce::String tooltip);
        ~ToggleChip() override;

        void paint (juce::Graphics&) override;
        void mouseEnter (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;
        void mouseDown (const juce::MouseEvent&) override;

        bool isOn() const noexcept { return on; }
        void toggle();

    private:
        juce::String label;
        bool on = false, hovering = false;
        juce::ParameterAttachment attachment;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToggleChip)
    };
}
