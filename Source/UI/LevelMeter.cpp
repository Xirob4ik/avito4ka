#include "LevelMeter.h"

#include "Fonts.h"
#include "Theme.h"

namespace clipline::ui
{
    using namespace theme;

    juce::String formatLevelDb (float db)
    {
        if (db <= -99.0f)
            return juce::String (juce::CharPointer_UTF8 ("-\xe2\x88\x9e"));       // -infinity

        float r = std::round (db * 10.0f) / 10.0f;
        if (std::abs (r) < 0.05f)
            r = 0.0f;                                   // never print "-0.0"
        return (r > 0.0f ? "+" : "") + juce::String (r, 1);
    }

    //==============================================================================================
    LevelMeter::LevelMeter (Orientation o, float lo, float hi) : orientation (o), minDb (lo), maxDb (hi)
    {
        setInterceptsMouseClicks (false, false);
    }

    float LevelMeter::fractionFor (float db) const noexcept
    {
        return juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb));
    }

    void LevelMeter::setValues (const Values& v)
    {
        if (! (v == values))
        {
            values = v;
            repaint();
        }
    }

    void LevelMeter::setCaption (juce::String c)
    {
        caption = std::move (c);
        repaint();
    }

    void LevelMeter::setReadout (const juce::String& r)
    {
        if (r != readout)
        {
            readout = r;
            repaint();
        }
    }

    void LevelMeter::drawBar (juce::Graphics& g, juce::Rectangle<float> bar, float levelDb, float holdDb) const
    {
        const bool vertical = orientation == Orientation::Vertical;
        const float radius = vertical ? bar.getWidth() * 0.5f : bar.getHeight() * 0.5f;

        g.setColour (surfaceHi);
        g.fillRoundedRectangle (bar, radius);

        // Fill up to the level; the part above 0 dB is drawn in the warning colour.
        const float fLevel = fractionFor (levelDb);
        const float fZero  = fractionFor (0.0f);
        auto cell = [&] (float f0, float f1) -> juce::Rectangle<float>
        {
            if (vertical)
                return { bar.getX(), bar.getBottom() - f1 * bar.getHeight(), bar.getWidth(), (f1 - f0) * bar.getHeight() };
            return { bar.getX() + f0 * bar.getWidth(), bar.getY(), (f1 - f0) * bar.getWidth(), bar.getHeight() };
        };

        if (fLevel > 0.0f)
        {
            juce::Graphics::ScopedSaveState state (g);
            juce::Path clip;
            clip.addRoundedRectangle (bar, radius);
            g.reduceClipRegion (clip);

            g.setColour (text.withAlpha (0.85f));
            g.fillRect (cell (0.0f, std::min (fLevel, fZero)));
            if (fLevel > fZero)
            {
                g.setColour (warn);
                g.fillRect (cell (fZero, fLevel));
            }
        }

        if (holdDb > minDb + 1.0f)
        {
            const float f = fractionFor (holdDb);
            g.setColour (holdDb > 0.0f ? warn : text);
            if (vertical)
                g.fillRect (bar.getX(), bar.getBottom() - f * bar.getHeight() - 1.0f, bar.getWidth(), 2.0f);
            else
                g.fillRect (bar.getX() + f * bar.getWidth() - 1.0f, bar.getY(), 2.0f, bar.getHeight());
        }
    }

    void LevelMeter::paint (juce::Graphics& g)
    {
        auto b = getLocalBounds().toFloat();

        if (orientation == Orientation::Vertical)
        {
            const float gap = 3.0f;
            const float w = (b.getWidth() - gap) * 0.5f;
            drawBar (g, { b.getX(), b.getY(), w, b.getHeight() }, values.levelDb[0], values.holdDb[0]);
            drawBar (g, { b.getX() + w + gap, b.getY(), w, b.getHeight() }, values.levelDb[1], values.holdDb[1]);
            return;
        }

        if (caption.isNotEmpty())
        {
            g.setColour (textDim);
            g.setFont (fonts::sans (10.0f, fonts::Weight::Medium, 0.16f));
            g.drawText (caption, b.removeFromLeft (22.0f), juce::Justification::centredLeft, false);
        }

        auto readoutArea = b.removeFromRight (readout.isNotEmpty() ? 38.0f : 0.0f);
        if (readout.isNotEmpty())
        {
            g.setColour (text);
            g.setFont (fonts::mono (11.0f));
            g.drawText (readout, readoutArea, juce::Justification::centredRight, false);
            b.removeFromRight (6.0f);
        }

        const float barH = 5.0f, gap = 3.0f;
        const float y0 = b.getCentreY() - barH - gap * 0.5f;
        drawBar (g, { b.getX(), y0, b.getWidth(), barH }, values.levelDb[0], values.holdDb[0]);
        drawBar (g, { b.getX(), y0 + barH + gap, b.getWidth(), barH }, values.levelDb[1], values.holdDb[1]);
    }

    //==============================================================================================
    MonitorPanel::MonitorPanel()
        : inMeter  (LevelMeter::Orientation::Vertical, kMinDb, kMaxDb),
          outMeter (LevelMeter::Orientation::Vertical, kMinDb, kMaxDb)
    {
        addAndMakeVisible (inMeter);
        addAndMakeVisible (outMeter);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTooltip ("Input / output peak meters (dBFS). Click to reset the peak values and the OVER latch.\n"
                    "CLIP lights while the clipper is cutting the signal and shows how far the loudest peak exceeded the threshold.");
    }

    void MonitorPanel::update (const Snapshot& s)
    {
        if (s == snapshot)
            return;

        snapshot = s;
        inMeter.setValues (s.input);
        outMeter.setValues (s.output);
        repaint();
    }

    void MonitorPanel::resized()
    {
        const auto b = getLocalBounds().toFloat();
        const float pad = 12.0f, meterW = 17.0f;
        const float top = 32.0f, h = 168.0f;
        inMeter.setBounds  (juce::Rectangle<float> (b.getX() + pad, top, meterW, h).toNearestInt());
        outMeter.setBounds (juce::Rectangle<float> (b.getRight() - pad - meterW, top, meterW, h).toNearestInt());
    }

    void MonitorPanel::mouseDown (const juce::MouseEvent&)
    {
        if (onResetRequested)
            onResetRequested();
    }

    void MonitorPanel::paint (juce::Graphics& g)
    {
        const auto b = getLocalBounds().toFloat();
        g.setColour (surface);
        g.fillRoundedRectangle (b, 8.0f);

        const auto inB = inMeter.getBounds().toFloat();
        const auto outB = outMeter.getBounds().toFloat();

        // ---- titles ----------------------------------------------------------------------------------
        g.setColour (textDim);
        g.setFont (fonts::sans (10.0f, fonts::Weight::Medium, 0.16f));
        g.drawText ("IN", juce::Rectangle<float> (inB.getX() - 6.0f, 12.0f, inB.getWidth() + 12.0f, 14.0f), juce::Justification::centred, false);
        g.drawText ("OUT", juce::Rectangle<float> (outB.getX() - 6.0f, 12.0f, outB.getWidth() + 12.0f, 14.0f), juce::Justification::centred, false);

        // ---- scale -------------------------------------------------------------------------------------
        const auto scaleArea = juce::Rectangle<float> (inB.getRight() + 2.0f, inB.getY(), outB.getX() - inB.getRight() - 4.0f, inB.getHeight());
        g.setFont (fonts::mono (9.0f));
        for (float db : { 0.0f, -6.0f, -12.0f, -24.0f, -36.0f, -48.0f })
        {
            const float y = inB.getBottom() - inMeter.fractionFor (db) * inB.getHeight();
            g.setColour (juce::exactlyEqual (db, 0.0f) ? textDim : textFaint);
            g.drawText (juce::String ((int) db), scaleArea.withY (y - 6.0f).withHeight (12.0f), juce::Justification::centred, false);
            g.setColour (hairlineSoft);
            g.fillRect (inB.getX() - 4.0f, y - 0.5f, 3.0f, 1.0f);
            g.fillRect (outB.getRight() + 1.0f, y - 0.5f, 3.0f, 1.0f);
        }

        // ---- peak readouts --------------------------------------------------------------------------------
        const float peakY = inB.getBottom() + 8.0f;
        auto peakText = [&] (float db, juce::Rectangle<float> over)
        {
            g.setColour (db > 0.0f ? warn : text);
            g.setFont (fonts::mono (10.5f));
            g.drawText (formatLevelDb (db), juce::Rectangle<float> (over.getCentreX() - 22.0f, peakY, 44.0f, 14.0f), juce::Justification::centred, false);
        };
        peakText (snapshot.inputMaxDb, inB);
        peakText (snapshot.outputMaxDb, outB);
        g.setColour (textFaint);
        g.setFont (fonts::sans (8.5f, fonts::Weight::Medium, 0.16f));
        g.drawText ("PEAK", scaleArea.withY (peakY).withHeight (14.0f), juce::Justification::centred, false);

        // ---- clipping activity / overload --------------------------------------------------------------------
        g.setColour (hairlineSoft);
        g.fillRect (b.getX() + 12.0f, peakY + 24.0f, b.getWidth() - 24.0f, 1.0f);

        auto led = [&] (float y, const char* name, bool lit, juce::Colour colour, const juce::String& value)
        {
            const float cx = b.getX() + 17.0f;
            if (lit)
            {
                g.setColour (colour);
                g.fillEllipse (cx - 4.0f, y + 3.0f, 8.0f, 8.0f);
            }
            else
            {
                g.setColour (hairline);
                g.drawEllipse (cx - 3.5f, y + 3.5f, 7.0f, 7.0f, 1.0f);
            }

            g.setColour (lit ? colour : textDim);
            g.setFont (fonts::sans (10.0f, fonts::Weight::SemiBold, 0.14f));
            g.drawText (name, juce::Rectangle<float> (b.getX() + 28.0f, y, 40.0f, 14.0f), juce::Justification::centredLeft, false);

            if (value.isNotEmpty())
            {
                g.setColour (lit ? colour : textDim);
                g.setFont (fonts::mono (10.0f));
                g.drawText (value, juce::Rectangle<float> (b.getRight() - 46.0f, y, 36.0f, 14.0f), juce::Justification::centredRight, false);
            }
        };

        const float ledY = peakY + 34.0f;
        led (ledY, "CLIP", snapshot.clipActive, accent,
             snapshot.clipHoldDb > -99.0f && snapshot.clipHoldDb > 0.0f ? formatLevelDb (snapshot.clipHoldDb) : juce::String());
        led (ledY + 22.0f, "OVER", snapshot.overLatched, warn, {});
    }
}
