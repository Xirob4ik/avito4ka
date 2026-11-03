#pragma once

#include <juce_graphics/juce_graphics.h>

namespace clipline::ui::fonts
{
    /** Owns the embedded typefaces (Geist and Geist Mono, SIL OFL 1.1). Reference counted through
        juce::SharedResourcePointer: the editor holds one, so the fonts exist exactly while a UI exists
        and are released before JUCE shuts down. */
    class FontLibrary
    {
    public:
        FontLibrary();
        ~FontLibrary();

        juce::Typeface::Ptr regular, medium, semiBold, monoRegular, monoMedium;
    };

    enum class Weight { Regular, Medium, SemiBold };

    /** Proportional UI font. `tracking` is extra letter spacing as a fraction of the font height. */
    juce::Font sans (float height, Weight weight = Weight::Regular, float tracking = 0.0f);

    /** Monospaced font with tabular digits, used for every number. */
    juce::Font mono (float height, bool medium = false);

    float textWidth (const juce::Font& font, const juce::String& text);
}
