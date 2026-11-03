#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace clipline::ui
{
    /** Dark, flat look for the parts of the UI that JUCE draws itself: popup menus, tooltips, dialogs,
        text entry and the window resize corner. Controls with their own paint() do not use it. */
    class CustomLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        CustomLookAndFeel();

        juce::Font getPopupMenuFont() override;
        void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
        void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                                bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                                const juce::String& shortcutKeyText, const juce::Drawable* icon,
                                const juce::Colour* textColour) override;
        void drawPopupMenuSectionHeader (juce::Graphics&, const juce::Rectangle<int>& area, const juce::String& sectionName) override;
        void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int standardMenuItemHeight,
                                        int& idealWidth, int& idealHeight) override;

        juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;
        void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;

        void drawCornerResizer (juce::Graphics&, int w, int h, bool isMouseOver, bool isMouseDragging) override;
        juce::Font getAlertWindowMessageFont() override;
        juce::Font getAlertWindowTitleFont() override;
        juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    };
}
