#pragma once

#include <juce_graphics/juce_graphics.h>

#include "../Branding.h"
#include "Fonts.h"
#include "Theme.h"

// Placeholder identity: a sine wave with its peaks cut off, plus a spaced wordmark. Both are drawn
// in code, so replacing them means editing this one file (see docs/BRANDING.md).
namespace clipline::ui::logo
{
    /** Draws the glyph inside `area` (any aspect, drawn as a square-ish mark). */
    inline void drawGlyph (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
    {
        const auto r = area.reduced (1.0f);
        const float cy = r.getCentreY();
        const float amp = r.getHeight() * 0.5f;
        const float ceiling = 0.58f;

        juce::Path wave;
        constexpr int steps = 40;
        for (int i = 0; i <= steps; ++i)
        {
            const float t = (float) i / (float) steps;
            const float v = juce::jlimit (-ceiling, ceiling, 1.45f * std::sin (t * juce::MathConstants<float>::twoPi));
            const float x = r.getX() + t * r.getWidth();
            const float y = cy - v * amp;
            if (i == 0) wave.startNewSubPath (x, y); else wave.lineTo (x, y);
        }

        g.setColour (colour.withAlpha (0.28f));
        g.drawHorizontalLine ((int) std::round (cy - ceiling * amp), r.getX(), r.getRight());
        g.drawHorizontalLine ((int) std::round (cy + ceiling * amp), r.getX(), r.getRight());

        g.setColour (colour);
        g.strokePath (wave, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    inline void drawWordmark (juce::Graphics& g, juce::Rectangle<float> area)
    {
        g.setColour (theme::text);
        g.setFont (fonts::sans (12.5f, fonts::Weight::SemiBold, 0.22f));
        g.drawText (branding::kWordmark, area, juce::Justification::centredLeft, false);
    }
}
