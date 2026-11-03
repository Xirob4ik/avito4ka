#include "SegmentedControl.h"

#include "Fonts.h"
#include "Theme.h"

namespace clipline::ui
{
    using namespace theme;

    SegmentedControl::SegmentedControl (juce::RangedAudioParameter& p, juce::StringArray l, juce::StringArray t)
        : labels (std::move (l)),
          tooltips (std::move (t)),
          attachment (p, [this] (float plain)
                      {
                          selected = juce::jlimit (0, juce::jmax (0, labels.size() - 1), (int) std::lround (plain));
                          repaint();
                      })
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        attachment.sendInitialUpdate();
    }

    SegmentedControl::~SegmentedControl() = default;

    juce::Rectangle<float> SegmentedControl::getSegmentBounds (int index) const noexcept
    {
        const auto b = getLocalBounds().toFloat();
        const float w = b.getWidth() / (float) juce::jmax (1, labels.size());
        return { b.getX() + w * (float) index, b.getY(), w, b.getHeight() };
    }

    int SegmentedControl::indexAt (juce::Point<float> p) const noexcept
    {
        if (! getLocalBounds().toFloat().contains (p) || labels.isEmpty())
            return -1;

        const float w = (float) getWidth() / (float) labels.size();
        return juce::jlimit (0, labels.size() - 1, (int) (p.x / w));
    }

    void SegmentedControl::paint (juce::Graphics& g)
    {
        const auto b = getLocalBounds().toFloat();
        g.setColour (surfaceHi);
        g.fillRoundedRectangle (b, 6.0f);

        for (int i = 0; i < labels.size(); ++i)
        {
            const auto seg = getSegmentBounds (i);
            const bool on = i == selected;
            const bool hot = i == hovered;

            if (on)
            {
                const auto inner = seg.reduced (3.0f);
                g.setColour (accent.withAlpha (0.14f));
                g.fillRoundedRectangle (inner, 4.0f);
                g.setColour (accent.withAlpha (0.55f));
                g.drawRoundedRectangle (inner.reduced (0.5f), 4.0f, 1.0f);
            }

            g.setColour (on ? accent : (hot ? text : textDim));
            g.setFont (fonts::sans (11.0f, on ? fonts::Weight::SemiBold : fonts::Weight::Medium, 0.12f));
            g.drawText (labels[i], seg, juce::Justification::centred, false);
        }
    }

    void SegmentedControl::mouseMove (const juce::MouseEvent& e)
    {
        const int idx = indexAt (e.position);
        if (idx != hovered)
        {
            hovered = idx;
            repaint();
        }
    }

    void SegmentedControl::mouseExit (const juce::MouseEvent&)
    {
        hovered = -1;
        repaint();
    }

    void SegmentedControl::mouseDown (const juce::MouseEvent& e)
    {
        if (e.mods.isPopupMenu())
            return;

        const int idx = indexAt (e.position);
        if (idx >= 0)
            select (idx);
    }

    void SegmentedControl::select (int index)
    {
        index = juce::jlimit (0, labels.size() - 1, index);
        if (index != selected)
            attachment.setValueAsCompleteGesture ((float) index);
    }

    juce::String SegmentedControl::getTooltip()
    {
        const int idx = indexAt (getMouseXYRelative().toFloat());
        return idx >= 0 && idx < tooltips.size() ? tooltips[idx] : juce::String();
    }
}
