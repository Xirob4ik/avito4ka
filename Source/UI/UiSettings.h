#pragma once

#include <juce_core/juce_core.h>

namespace clipline::ui::settings
{
    /** Per-user interface preferences (not part of the plugin state, so they never end up in a DAW project):
        window size, waveform time window, tooltips. Stored as XML in <user app data>/Clipline/ui.settings. */
    float getWindowMs();
    void  setWindowMs (float ms);

    bool  getTooltipsEnabled();
    void  setTooltipsEnabled (bool);

    /** Windows only: draw with JUCE's software renderer instead of Direct2D (escape hatch for graphics drivers
        that render the interface incorrectly). */
    bool  getSoftwareRenderer();
    void  setSoftwareRenderer (bool);

    int   getEditorWidth();             // logical window width in pixels (height follows the aspect ratio)
    void  setEditorWidth (int width);
}
