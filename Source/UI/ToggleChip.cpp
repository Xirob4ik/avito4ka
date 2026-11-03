#include "ToggleChip.h"

#include "Fonts.h"
#include "Theme.h"

namespace clipline::ui
{
    using namespace theme;

    ToggleChip::ToggleChip (juce::RangedAudioParameter& p, juce::String l, juce::String tip)
        : label (std::move (l)),
          attachment (p, [this] (float plain)
                      {
                          on = plain > 0.5f;
                          repaint();
                      })
    {
        setTooltip (std::move (tip));
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        attachment.sendInitialUpdate();
    }

    ToggleChip::~ToggleChip() = default;

    void ToggleChip::paint (juce::Graphics& g)
    {
        const auto b = getLocalBounds().toFloat();

        g.setColour (surfaceHi);
        g.fillRoundedRectangle (b, 6.0f);
        if (on)
        {
            g.setColour (accent.withAlpha (0.55f));
            g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);
        }
        else if (hovering)
        {
            g.setColour (hairline.brighter (0.2f));
            g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);
        }

        const float dot = 7.0f;
        const float dx = b.getX() + 12.0f, dy = b.getCentreY() - dot * 0.5f;
        if (on)
        {
            g.setColour (accent);
            g.fillEllipse (dx, dy, dot, dot);
        }
        else
        {
            g.setColour (textFaint);
            g.drawEllipse (dx + 0.5f, dy + 0.5f, dot - 1.0f, dot - 1.0f, 1.0f);
        }

        g.setColour (on ? accent : (hovering ? text : textDim));
        g.setFont (fonts::sans (11.0f, on ? fonts::Weight::SemiBold : fonts::Weight::Medium, 0.14f));
        g.drawText (label, b.withTrimmedLeft (28.0f), juce::Justification::centredLeft, false);
    }

    void ToggleChip::mouseEnter (const juce::MouseEvent&) { hovering = true;  repaint(); }
    void ToggleChip::mouseExit (const juce::MouseEvent&)  { hovering = false; repaint(); }

    void ToggleChip::mouseDown (const juce::MouseEvent& e)
    {
        if (! e.mods.isPopupMenu())
            toggle();
    }

    void ToggleChip::toggle()
    {
        attachment.setValueAsCompleteGesture (on ? 0.0f : 1.0f);
    }
}
