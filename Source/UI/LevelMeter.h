#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace clipline::ui
{
    /** Two-channel peak meter (vertical or horizontal) with a peak-hold marker per channel.
        Pure view: the editor feeds it dB values computed from the audio thread's lock-free meter data. */
    class LevelMeter : public juce::Component
    {
    public:
        enum class Orientation { Vertical, Horizontal };

        struct Values
        {
            float levelDb[2] { -100.0f, -100.0f };
            float holdDb[2]  { -100.0f, -100.0f };
            bool operator== (const Values& o) const noexcept
            {
                return juce::exactlyEqual (levelDb[0], o.levelDb[0]) && juce::exactlyEqual (levelDb[1], o.levelDb[1])
                    && juce::exactlyEqual (holdDb[0], o.holdDb[0])   && juce::exactlyEqual (holdDb[1], o.holdDb[1]);
            }
        };

        LevelMeter (Orientation, float minDb, float maxDb);

        void setValues (const Values&);
        const Values& getValues() const noexcept { return values; }

        /** Horizontal meters only: text drawn before the bars / after the bars. */
        void setCaption (juce::String);
        void setReadout (const juce::String&);

        void paint (juce::Graphics&) override;

        /** Maps a dB value to 0..1 along the meter. */
        float fractionFor (float db) const noexcept;

    private:
        void drawBar (juce::Graphics&, juce::Rectangle<float> bar, float levelDb, float holdDb) const;

        Orientation orientation;
        float minDb, maxDb;
        Values values;
        juce::String caption, readout;
    };

    //==============================================================================================
    /** Region D: input / output meters with scale, peak values, clipping activity and overload. */
    class MonitorPanel : public juce::Component,
                         public juce::SettableTooltipClient
    {
    public:
        struct Snapshot
        {
            LevelMeter::Values input, output;
            float inputMaxDb = -100.0f, outputMaxDb = -100.0f;     // session peaks (numeric readouts)
            bool clipActive = false;                               // clipper is limiting right now
            float clipHoldDb = -100.0f;                            // largest overshoot of the threshold, dB
            bool overLatched = false;                              // output exceeded 0 dBFS (click to clear)

            bool operator== (const Snapshot& o) const noexcept
            {
                return input == o.input && output == o.output
                    && juce::exactlyEqual (inputMaxDb, o.inputMaxDb) && juce::exactlyEqual (outputMaxDb, o.outputMaxDb)
                    && clipActive == o.clipActive && juce::exactlyEqual (clipHoldDb, o.clipHoldDb) && overLatched == o.overLatched;
            }
        };

        MonitorPanel();

        void update (const Snapshot&);
        const Snapshot& getSnapshot() const noexcept { return snapshot; }

        void resized() override;
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;

        /** Called when the user clicks the panel: clears peak values and the overload latch. */
        std::function<void()> onResetRequested;

        static constexpr float kMinDb = -48.0f, kMaxDb = 6.0f;

    private:
        LevelMeter inMeter, outMeter;
        Snapshot snapshot;
    };

    juce::String formatLevelDb (float db);      // "-6.2", "+1.3", "-inf"
}
