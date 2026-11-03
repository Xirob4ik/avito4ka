#include "WaveformDisplay.h"

#include <algorithm>
#include <cmath>

#include "../ParameterIds.h"
#include "Fonts.h"
#include "Theme.h"

namespace clipline::ui
{
    using namespace theme;


    namespace
    {
        constexpr float kGutterLeft = 40.0f;
        constexpr float kGutterRight = 66.0f;
        constexpr float kTop = 16.0f;
        constexpr float kBottom = 34.0f;
        constexpr float kHitTolerance = 7.0f;

        float dbToLinear (float db) { return std::pow (10.0f, db * 0.05f); }
        float linearToDb (float a)  { return 20.0f * std::log10 (std::max (a, 1.0e-5f)); }
    }

    WaveformDisplay::WaveformDisplay (clipline::dsp::WaveformTap& t, juce::RangedAudioParameter& p)
        : tap (t),
          clipParameter (p),
          attachment (p, [this] (float plain)
                      {
                          clipDb = plain;
                          dirty = true;
                          repaint();
                      })
    {
        setTooltip ("Signal entering the clipper (grey) and the clipped result (green). Shaded areas are being cut off.\n"
                    "Drag the threshold line to set CLIP, double-click it to reset. Mouse wheel zooms the time axis.");
        attachment.sendInitialUpdate();
    }

    WaveformDisplay::~WaveformDisplay() = default;

    //==============================================================================================
    juce::Rectangle<float> WaveformDisplay::getPlotBounds() const noexcept
    {
        auto b = getLocalBounds().toFloat();
        return b.withTrimmedLeft (kGutterLeft).withTrimmedRight (kGutterRight).withTrimmedTop (kTop).withTrimmedBottom (kBottom);
    }

    float WaveformDisplay::yForAmplitude (float a) const noexcept
    {
        const auto plot = getPlotBounds();
        return plot.getCentreY() - juce::jlimit (-kRange, kRange, a) / kRange * (plot.getHeight() * 0.5f);
    }

    float WaveformDisplay::amplitudeForY (float y) const noexcept
    {
        const auto plot = getPlotBounds();
        return (plot.getCentreY() - y) / (plot.getHeight() * 0.5f) * kRange;
    }

    float WaveformDisplay::getThresholdLinear() const noexcept
    {
        return dbToLinear (clipDb);
    }

    bool WaveformDisplay::isOverThresholdLine (juce::Point<float> p) const noexcept
    {
        const auto plot = getPlotBounds();
        if (p.x < plot.getX() - 4.0f || p.x > (float) getWidth() - 2.0f)
            return false;

        const float T = getThresholdLinear();
        return std::abs (p.y - yForAmplitude (T)) <= kHitTolerance || std::abs (p.y - yForAmplitude (-T)) <= kHitTolerance;
    }

    void WaveformDisplay::setWindowMs (float ms)
    {
        ms = juce::jlimit (kWindowChoicesMs[0], kWindowChoicesMs[4], ms);
        if (! juce::exactlyEqual (ms, windowMs))
        {
            windowMs = ms;
            dirty = true;
            if (onWindowMsChanged)
                onWindowMsChanged (windowMs);
            repaint();
        }
    }

    void WaveformDisplay::resized()
    {
        dirty = true;
    }

    //==============================================================================================
    bool WaveformDisplay::refresh()
    {
        const auto writeCount = tap.getWriteCount();
        if (writeCount == lastWriteCount && ! dirty)
            return false;

        lastWriteCount = writeCount;
        dirty = false;

        const float colSeconds = juce::jmax (1.0e-6f, tap.getColumnSeconds());
        windowCols = juce::jlimit (32, 4096, (int) std::lround (windowMs * 0.001f / colSeconds));

        const int wanted = windowCols * 2 + 16;
        if ((int) scratch.size() != wanted)
            scratch.assign ((size_t) wanted, {});
        if ((int) display.size() != windowCols)
            display.assign ((size_t) windowCols, {});

        std::uint64_t end = 0;
        const int n = tap.copyLatest (scratch.data(), wanted, end);

        // Trigger: the newest rising zero crossing of the (pre-clip) signal that still leaves a full window.
        // This makes periodic material stand still instead of scrolling.
        const int lead = windowCols / 12;
        int start = juce::jmax (0, n - windowCols);

        if (n >= windowCols)
        {
            float lo = 1.0e9f, hi = -1.0e9f;
            for (int i = n - windowCols; i < n; ++i)
            {
                lo = std::min (lo, scratch[(size_t) i].inMin);
                hi = std::max (hi, scratch[(size_t) i].inMax);
            }

            if (hi - lo > 0.02f)
            {
                auto mid = [&] (int i) { return 0.5f * (scratch[(size_t) i].inMin + scratch[(size_t) i].inMax); };
                for (int i = std::min (n - 1, n - windowCols + lead); i >= 1; --i)
                {
                    if (mid (i - 1) < 0.0f && mid (i) >= 0.0f)
                    {
                        start = juce::jmax (0, i - lead);
                        break;
                    }
                }
            }
        }

        displayCount = juce::jmax (0, std::min (windowCols, n - start));
        for (int i = 0; i < displayCount; ++i)
            display[(size_t) i] = scratch[(size_t) (start + i)];

        repaint();
        return true;
    }

    void WaveformDisplay::buildBands (std::vector<Band>& in, std::vector<Band>& out, juce::Rectangle<float> plot) const
    {
        in.clear();
        out.clear();

        const int count = displayCount;
        if (count < 2)
            return;

        const float colW = plot.getWidth() / (float) windowCols;
        const int points = std::min (count, (int) std::ceil (plot.getWidth()));

        for (int j = 0; j < points; ++j)
        {
            const int c0 = j * count / points;
            const int c1 = std::max (c0 + 1, (j + 1) * count / points);

            float inLo = 1.0e9f, inHi = -1.0e9f, outLo = 1.0e9f, outHi = -1.0e9f;
            for (int c = c0; c < c1; ++c)
            {
                const auto& col = display[(size_t) c];
                inLo = std::min (inLo, col.inMin);   inHi = std::max (inHi, col.inMax);
                outLo = std::min (outLo, col.outMin); outHi = std::max (outHi, col.outMax);
            }

            const float x = plot.getX() + 0.5f * (float) (c0 + c1) * colW;
            in.push_back ({ x, inLo, inHi });
            out.push_back ({ x, outLo, outHi });
        }
    }

    //==============================================================================================
    void WaveformDisplay::paint (juce::Graphics& g)
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour (surface);
        g.fillRoundedRectangle (bounds, 8.0f);

        const auto plot = getPlotBounds();
        const float T = getThresholdLinear();
        const float yT = yForAmplitude (T), yNT = yForAmplitude (-T);
        const bool hot = hovering || dragging;

        // ---- cut-off zones (beyond the threshold) -------------------------------------------------------
        g.setColour (accent.withAlpha (0.045f));
        g.fillRect (plot.getX(), plot.getY(), plot.getWidth(), yT - plot.getY());
        g.fillRect (plot.getX(), yNT, plot.getWidth(), plot.getBottom() - yNT);

        // ---- grid ----------------------------------------------------------------------------------------
        g.setColour (hairlineSoft);
        for (int i = 1; i < 4; ++i)
            g.fillRect (plot.getX() + plot.getWidth() * (float) i * 0.25f - 0.5f, plot.getY(), 1.0f, plot.getHeight());

        struct Mark { float amplitude; const char* label; };
        for (const auto& m : { Mark { 1.0f, "0" }, Mark { 0.5f, "-6" }, Mark { 0.25f, "-12" } })
        {
            for (float sign : { 1.0f, -1.0f })
            {
                const float y = std::round (yForAmplitude (sign * m.amplitude));
                g.setColour (juce::exactlyEqual (m.amplitude, 1.0f) ? hairline : hairlineSoft);
                g.fillRect (plot.getX(), y - 0.5f, plot.getWidth(), 1.0f);
            }

            g.setColour (textFaint);
            g.setFont (fonts::mono (9.5f));
            g.drawText (m.label, juce::Rectangle<float> (4.0f, yForAmplitude (m.amplitude) - 6.0f, kGutterLeft - 12.0f, 12.0f),
                        juce::Justification::centredRight, false);
        }

        g.setColour (hairline);
        g.fillRect (plot.getX(), std::round (plot.getCentreY()) - 0.5f, plot.getWidth(), 1.0f);

        // ---- waveforms --------------------------------------------------------------------------------------
        buildBands (inBands, outBands, plot);
        if (inBands.size() >= 2)
        {
            juce::Graphics::ScopedSaveState state (g);
            g.reduceClipRegion (plot.expanded (0.0f, 1.0f).toNearestInt());

            auto envelope = [&] (const std::vector<Band>& bands)
            {
                juce::Path p;
                p.startNewSubPath (bands.front().x, yForAmplitude (bands.front().hi));
                for (const auto& b : bands)
                    p.lineTo (b.x, yForAmplitude (b.hi));
                for (auto it = bands.rbegin(); it != bands.rend(); ++it)
                    p.lineTo (it->x, yForAmplitude (it->lo));
                p.closeSubPath();
                return p;
            };

            // input
            const auto inPath = envelope (inBands);
            g.setColour (text.withAlpha (0.08f));
            g.fillPath (inPath);
            g.setColour (text.withAlpha (0.5f));
            g.strokePath (inPath, juce::PathStrokeType (1.0f, juce::PathStrokeType::curved));

            // the parts being cut off
            const float sliver = std::max (1.0f, plot.getWidth() / (float) inBands.size());
            juce::Path cut;
            for (const auto& b : inBands)
            {
                if (b.hi > T)
                    cut.addRectangle (b.x - sliver * 0.5f, yForAmplitude (b.hi), sliver, yT - yForAmplitude (b.hi));
                if (b.lo < -T)
                    cut.addRectangle (b.x - sliver * 0.5f, yNT, sliver, yForAmplitude (b.lo) - yNT);
            }
            g.setColour (accent.withAlpha (0.34f));
            g.fillPath (cut);

            // output
            const auto outPath = envelope (outBands);
            g.setColour (accent.withAlpha (0.11f));
            g.fillPath (outPath);
            g.setColour (accent);
            g.strokePath (outPath, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // ---- threshold lines and handle -----------------------------------------------------------------------
        const float lineW = hot ? 1.6f : 1.0f;
        g.setColour (accent.withAlpha (hot ? 1.0f : 0.85f));
        for (float y : { yT, yNT })
            g.fillRect (plot.getX(), std::round (y) - lineW * 0.5f, (float) getWidth() - plot.getX() - 8.0f, lineW);

        const auto tag = juce::Rectangle<float> (plot.getRight() + 8.0f, yT - 9.5f, kGutterRight - 14.0f, 19.0f);
        g.setColour (accent);
        g.fillRoundedRectangle (tag, 4.0f);
        g.setColour (bg);
        g.setFont (fonts::mono (10.5f, true));
        g.drawText (juce::String (std::round (clipDb * 10.0f) / 10.0f, 1) + " dB", tag, juce::Justification::centred, false);

        // ---- legend ------------------------------------------------------------------------------------------------
        const float ly = bounds.getBottom() - 21.0f;
        float lx = plot.getX();
        g.setFont (fonts::sans (10.0f, fonts::Weight::Medium, 0.14f));

        auto legendText = [&] (const juce::String& s, juce::Colour c)
        {
            const float w = fonts::textWidth (g.getCurrentFont(), s) + 2.0f;
            g.setColour (c);
            g.drawText (s, juce::Rectangle<float> (lx, ly - 1.0f, w, 14.0f), juce::Justification::centredLeft, false);
            lx += w + 16.0f;
        };

        g.setColour (text.withAlpha (0.5f));
        g.fillRect (lx, ly + 6.0f, 14.0f, 1.5f);
        lx += 20.0f;
        legendText ("IN", textDim);

        g.setColour (accent);
        g.fillRect (lx, ly + 6.0f, 14.0f, 2.0f);
        lx += 20.0f;
        legendText ("OUT", textDim);

        g.setColour (accent.withAlpha (0.34f));
        g.fillRect (lx, ly + 2.0f, 12.0f, 9.0f);
        lx += 18.0f;
        legendText ("CUT", textDim);

        g.setColour (textFaint);
        g.setFont (fonts::mono (10.0f));
        g.drawText (juce::String ((int) std::lround (windowMs)) + " ms", juce::Rectangle<float> (plot.getRight() - 80.0f, ly - 1.0f, 80.0f, 14.0f),
                    juce::Justification::centredRight, false);
    }

    //==============================================================================================
    void WaveformDisplay::mouseMove (const juce::MouseEvent& e)
    {
        const bool over = isOverThresholdLine (e.position);
        if (over != hovering)
        {
            hovering = over;
            setMouseCursor (over ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
            repaint();
        }
    }

    void WaveformDisplay::mouseExit (const juce::MouseEvent&)
    {
        if (! dragging && hovering)
        {
            hovering = false;
            setMouseCursor (juce::MouseCursor::NormalCursor);
            repaint();
        }
    }

    void WaveformDisplay::mouseDown (const juce::MouseEvent& e)
    {
        if (e.mods.isPopupMenu() || ! isOverThresholdLine (e.position))
            return;

        dragging = true;
        dragStartY = e.position.y;
        dragStartAmplitude = getThresholdLinear();
        attachment.beginGesture();
        repaint();
    }

    void WaveformDisplay::applyThresholdFromY (float y, bool fine, float startY, float startAmplitude)
    {
        // Distance from the centre line decides the threshold, so grabbing either of the two symmetric
        // lines works the same way. Shift / Ctrl / Cmd moves the line at one tenth of the mouse speed.
        float amplitude = std::abs (amplitudeForY (y));
        if (fine)
            amplitude = startAmplitude + (amplitude - std::abs (amplitudeForY (startY))) * 0.1f;

        float db = juce::jlimit (clipline::params::kClipMinDb, clipline::params::kClipMaxDb, linearToDb (amplitude));
        db = clipParameter.getNormalisableRange().snapToLegalValue (db);
        attachment.setValueAsPartOfGesture (db);
    }

    void WaveformDisplay::mouseDrag (const juce::MouseEvent& e)
    {
        if (! dragging)
            return;

        const bool fine = e.mods.isShiftDown() || e.mods.isCtrlDown() || e.mods.isCommandDown();
        applyThresholdFromY (e.position.y, fine, dragStartY, dragStartAmplitude);
    }

    void WaveformDisplay::mouseUp (const juce::MouseEvent& e)
    {
        if (dragging)
        {
            dragging = false;
            attachment.endGesture();
            hovering = isOverThresholdLine (e.position);
            repaint();
        }
    }

    void WaveformDisplay::mouseDoubleClick (const juce::MouseEvent& e)
    {
        if (e.mods.isPopupMenu() || ! isOverThresholdLine (e.position))
            return;

        const float def = clipParameter.convertFrom0to1 (clipParameter.getDefaultValue());
        if (dragging)
            attachment.setValueAsPartOfGesture (def);
        else
            attachment.setValueAsCompleteGesture (def);
    }

    void WaveformDisplay::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
    {
        wheelAccumulator += wheel.deltaY * (wheel.isReversed ? -1.0f : 1.0f);
        if (std::abs (wheelAccumulator) < 0.1f)
            return;

        const int direction = wheelAccumulator > 0.0f ? 1 : -1;       // wheel up = zoom in = shorter window
        wheelAccumulator = 0.0f;

        int index = 0;
        for (int i = 0; i < 5; ++i)
            if (std::abs (kWindowChoicesMs[i] - windowMs) < std::abs (kWindowChoicesMs[index] - windowMs))
                index = i;

        setWindowMs (kWindowChoicesMs[juce::jlimit (0, 4, index - direction)]);
    }
}
