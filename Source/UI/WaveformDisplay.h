#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>
#include <vector>

#include "../DSP/WaveformTap.h"

namespace clipline::ui
{
    /** Region B: the clipper at work.

        Shows the real signal entering the clipper (grey), the clipped result (accent), the threshold as a
        draggable pair of lines, and shades the parts of the waveform that are being cut off.

        Data comes from clipline::dsp::WaveformTap, which the audio thread fills with min/max columns without
        locking. This component only reads it (copy of the newest columns, ~60 times per second), picks a
        rising zero crossing as trigger so periodic material stands still, and decimates to pixels. No DSP
        runs on the UI thread. */
    class WaveformDisplay : public juce::Component,
                            public juce::SettableTooltipClient
    {
    public:
        WaveformDisplay (clipline::dsp::WaveformTap& tap, juce::RangedAudioParameter& clipParameter);
        ~WaveformDisplay() override;

        /** Pulls new data. Returns true when the picture changed and a repaint was requested. */
        bool refresh();

        void setWindowMs (float ms);
        float getWindowMs() const noexcept { return windowMs; }
        std::function<void (float)> onWindowMsChanged;

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

        // ---- geometry (public for tests) -------------------------------------------------------------------
        static constexpr float kRange = 1.25f;                         // half-height of the plot in linear amplitude
        static constexpr float kWindowChoicesMs[] = { 5.0f, 10.0f, 20.0f, 40.0f, 80.0f };

        juce::Rectangle<float> getPlotBounds() const noexcept;
        float yForAmplitude (float amplitude) const noexcept;
        float amplitudeForY (float y) const noexcept;
        float getThresholdLinear() const noexcept;
        float getThresholdDb() const noexcept { return clipDb; }
        bool isOverThresholdLine (juce::Point<float> p) const noexcept;
        int getNumDisplayedColumns() const noexcept { return displayCount; }

    private:
        struct Band { float x, lo, hi; };

        void buildBands (std::vector<Band>& in, std::vector<Band>& out, juce::Rectangle<float> plot) const;
        void applyThresholdFromY (float y, bool fine, float startY, float startAmplitude);

        clipline::dsp::WaveformTap& tap;
        juce::RangedAudioParameter& clipParameter;
        juce::ParameterAttachment attachment;

        float clipDb = -3.0f;
        float windowMs = 20.0f;
        int windowCols = 320;

        std::vector<clipline::dsp::WaveformTap::Column> scratch, display;
        int displayCount = 0;
        std::uint64_t lastWriteCount = ~(std::uint64_t) 0;
        bool dirty = true;

        bool hovering = false, dragging = false;
        float dragStartY = 0.0f, dragStartAmplitude = 0.0f;
        float wheelAccumulator = 0.0f;

        mutable std::vector<Band> inBands, outBands;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformDisplay)
    };
}
