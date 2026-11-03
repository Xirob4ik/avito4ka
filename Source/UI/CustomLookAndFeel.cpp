#include "CustomLookAndFeel.h"

#include "Fonts.h"
#include "Theme.h"

namespace clipline::ui
{
    using namespace theme;

    CustomLookAndFeel::CustomLookAndFeel()
    {
        setColourScheme ({ bg, surface, surface, hairline, text, accent, bg, accent, text });

        setColour (juce::PopupMenu::backgroundColourId, surface);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::headerTextColourId, textDim);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, surfaceHi);
        setColour (juce::PopupMenu::highlightedTextColourId, accent);

        setColour (juce::TooltipWindow::textColourId, text);
        setColour (juce::TooltipWindow::backgroundColourId, surfaceHi);

        setColour (juce::TextEditor::backgroundColourId, surfaceHi);
        setColour (juce::TextEditor::textColourId, text);
        setColour (juce::TextEditor::outlineColourId, hairline);
        setColour (juce::TextEditor::focusedOutlineColourId, accent);
        setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.35f));
        setColour (juce::TextEditor::highlightedTextColourId, text);
        setColour (juce::CaretComponent::caretColourId, accent);

        setColour (juce::AlertWindow::backgroundColourId, surface);
        setColour (juce::AlertWindow::textColourId, text);
        setColour (juce::AlertWindow::outlineColourId, hairline);
        setColour (juce::TextButton::buttonColourId, surfaceHi);
        setColour (juce::TextButton::textColourOffId, text);
    }

    juce::Font CustomLookAndFeel::getPopupMenuFont()                              { return fonts::sans (12.5f, fonts::Weight::Regular); }
    juce::Font CustomLookAndFeel::getAlertWindowMessageFont()                     { return fonts::sans (12.5f); }
    juce::Font CustomLookAndFeel::getAlertWindowTitleFont()                       { return fonts::sans (15.0f, fonts::Weight::SemiBold); }
    juce::Font CustomLookAndFeel::getTextButtonFont (juce::TextButton&, int)      { return fonts::sans (12.5f, fonts::Weight::Medium); }

    void CustomLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
    {
        const auto r = juce::Rectangle<float> ((float) width, (float) height);
        g.setColour (surface);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (hairline);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    }

    void CustomLookAndFeel::getIdealPopupMenuItemSize (const juce::String& itemText, bool isSeparator, int,
                                                       int& idealWidth, int& idealHeight)
    {
        if (isSeparator)
        {
            idealWidth = 50;
            idealHeight = 9;
            return;
        }

        idealWidth = (int) std::ceil (fonts::textWidth (getPopupMenuFont(), itemText)) + 52;
        idealHeight = 26;
    }

    void CustomLookAndFeel::drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& name)
    {
        g.setColour (textFaint);
        g.setFont (fonts::sans (9.5f, fonts::Weight::Medium, 0.16f));
        g.drawText (name.toUpperCase(), area.reduced (14, 0).withTrimmedTop (6), juce::Justification::centredLeft, false);
    }

    void CustomLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                                               bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& itemText,
                                               const juce::String& shortcutKeyText, const juce::Drawable*, const juce::Colour* textColour)
    {
        if (isSeparator)
        {
            g.setColour (hairline);
            g.fillRect (area.reduced (10, 0).withHeight (1).withY (area.getCentreY()));
            return;
        }

        auto row = area.reduced (4, 1).toFloat();
        if (isHighlighted && isActive)
        {
            g.setColour (surfaceHi);
            g.fillRoundedRectangle (row, 4.0f);
        }

        auto colour = textColour != nullptr ? *textColour : text;
        if (isTicked) colour = accent;
        if (isHighlighted && isActive) colour = isTicked ? accent : text;
        if (! isActive) colour = textFaint;

        auto inner = row.reduced (10.0f, 0.0f);
        g.setColour (colour);
        g.setFont (getPopupMenuFont());

        if (isTicked)
        {
            g.fillEllipse (inner.getX() - 1.0f, inner.getCentreY() - 2.5f, 5.0f, 5.0f);
        }

        g.drawText (itemText, inner.withTrimmedLeft (12.0f), juce::Justification::centredLeft, true);

        if (shortcutKeyText.isNotEmpty())
        {
            g.setColour (textDim);
            g.setFont (fonts::mono (11.0f));
            g.drawText (shortcutKeyText, inner, juce::Justification::centredRight, false);
        }

        if (hasSubMenu)
        {
            g.setColour (textDim);
            juce::Path chevron;
            const float cx = inner.getRight() - 4.0f, cy = inner.getCentreY();
            chevron.startNewSubPath (cx - 2.0f, cy - 4.0f);
            chevron.lineTo (cx + 2.0f, cy);
            chevron.lineTo (cx - 2.0f, cy + 4.0f);
            g.strokePath (chevron, juce::PathStrokeType (1.4f));
        }
    }

    juce::Rectangle<int> CustomLookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                                              juce::Rectangle<int> parentArea)
    {
        juce::AttributedString s;
        s.setJustification (juce::Justification::centredLeft);
        s.append (tipText, fonts::sans (11.5f), text);

        juce::TextLayout layout;
        layout.createLayout (s, 260.0f);

        const int w = (int) std::ceil (layout.getWidth()) + 20;
        const int h = (int) std::ceil (layout.getHeight()) + 14;

        return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 22,
                                     screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6)  : screenPos.y + 8,
                                     w, h)
                   .constrainedWithin (parentArea);
    }

    void CustomLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& tipText, int width, int height)
    {
        const auto r = juce::Rectangle<float> ((float) width, (float) height);
        g.setColour (surfaceHi);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (hairline);
        g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);

        g.setColour (text);
        g.setFont (fonts::sans (11.5f));
        g.drawFittedText (tipText, juce::Rectangle<int> (width, height).reduced (10, 7), juce::Justification::centredLeft, 6, 1.0f);
    }

    void CustomLookAndFeel::drawCornerResizer (juce::Graphics& g, int w, int h, bool isMouseOver, bool isMouseDragging)
    {
        g.setColour (isMouseOver || isMouseDragging ? textDim : textFaint);
        for (int i = 0; i < 3; ++i)
        {
            const float o = 4.0f + (float) i * 4.0f;
            g.drawLine ((float) w - o, (float) h - 2.0f, (float) w - 2.0f, (float) h - o, 1.0f);
        }
    }
}
