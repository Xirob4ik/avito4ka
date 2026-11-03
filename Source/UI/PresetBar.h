#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Presets/PresetManager.h"

namespace clipline::ui
{
    /** Compact preset selector: previous / name (opens the menu) / next. */
    class PresetBar : public juce::Component,
                      public juce::SettableTooltipClient
    {
    public:
        explicit PresetBar (PresetManager&);

        /** Re-reads the current name and the modified state. Call regularly (cheap); repaints on change. */
        void refresh();

        void paint (juce::Graphics&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;
        void mouseDown (const juce::MouseEvent&) override;

        void showMenu();
        const juce::String& getDisplayedName() const noexcept { return name; }
        bool isShowingModified() const noexcept { return modified; }

    private:
        enum Zone { none = -1, previous, label, next };
        Zone zoneAt (juce::Point<float>) const noexcept;
        void promptForName();

        PresetManager& manager;
        juce::String name;
        bool modified = false;
        int hot = none;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBar)
    };
}
