#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace clipline::ui
{
    /** A row of mutually exclusive options bound to a choice parameter (clipping mode, oversampling). */
    class SegmentedControl : public juce::Component,
                             public juce::TooltipClient
    {
    public:
        SegmentedControl (juce::RangedAudioParameter& parameter, juce::StringArray labels, juce::StringArray tooltips);
        ~SegmentedControl() override;

        void paint (juce::Graphics&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;
        void mouseDown (const juce::MouseEvent&) override;
        juce::String getTooltip() override;

        int getSelectedIndex() const noexcept { return selected; }
        int getNumSegments() const noexcept { return labels.size(); }
        juce::Rectangle<float> getSegmentBounds (int index) const noexcept;
        /** Selects a segment as one complete automation gesture. */
        void select (int index);

    private:
        int indexAt (juce::Point<float> p) const noexcept;

        juce::StringArray labels, tooltips;
        int selected = 0;
        int hovered = -1;
        juce::ParameterAttachment attachment;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SegmentedControl)
    };
}
