#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace clipline::ui
{
    /** Rotary control bound to one host parameter.

        Interaction
          - drag up / down to change (full range in ~220 px); hold Shift or Ctrl / Cmd for 10x finer steps
          - mouse wheel: ~1.5% of the range per notch (Shift: finer)
          - double-click the dial: reset to the parameter's default
          - click the numeric value: type an exact value, Enter to apply, Esc to cancel

        Every user change is wrapped in a begin/end gesture so hosts record automation correctly. */
    class KnobComponent : public juce::Component,
                          public juce::SettableTooltipClient
    {
    public:
        struct Style
        {
            bool primary = false;       // larger, accent coloured: the main control
            bool bipolar = false;       // arc is filled from the centre (0 dB)
            bool fillFromMax = false;   // arc is filled from the maximum end (CLIP: grows as the ceiling drops)
        };

        KnobComponent (juce::RangedAudioParameter& parameter, juce::String title, Style style, juce::String tooltip);
        ~KnobComponent() override;

        void paint (juce::Graphics&) override;
        void resized() override;

        void mouseEnter (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

        float getNormalised() const noexcept { return norm; }
        juce::Rectangle<float> getDialBounds() const noexcept { return dialBounds; }
        juce::Rectangle<float> getValueBounds() const noexcept { return valueBounds; }
        juce::RangedAudioParameter& getParameter() noexcept { return parameter; }

        void resetToDefault();
        void showTextEntry();
        bool isTextEntryOpen() const noexcept { return textEditor != nullptr; }
        /** Parses `text` like the text entry does and applies it as one complete gesture. */
        bool commitText (const juce::String& text);

    private:
        void setNormalisedAsPartOfGesture (float newNorm);
        void closeTextEntryAsync();
        bool isOnValue (juce::Point<float> p) const noexcept;

        juce::RangedAudioParameter& parameter;
        juce::String title;
        Style style;

        float norm = 0.0f;
        float dragNorm = 0.0f;
        float lastDragDistance = 0.0f;
        bool hovering = false, dragging = false, pressedOnValue = false;

        juce::Rectangle<float> titleBounds, dialBounds, valueBounds;
        std::unique_ptr<juce::TextEditor> textEditor;
        juce::ParameterAttachment attachment;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KnobComponent)
    };
}
