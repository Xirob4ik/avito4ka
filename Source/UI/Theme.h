#pragma once

#include <juce_graphics/juce_graphics.h>

// Design tokens. Every colour and metric of the interface is defined here; no other file contains a
// colour literal. One accent colour, one warning colour, neutral greys for everything else.
namespace clipline::ui::theme
{
    inline const juce::Colour bg          { 0xff101010 };   // window
    inline const juce::Colour surface     { 0xff181818 };   // panels
    inline const juce::Colour surfaceHi   { 0xff202020 };   // controls, wells
    inline const juce::Colour hairline    { 0xff2e2e2e };   // borders
    inline const juce::Colour hairlineSoft{ 0xff242424 };   // grids

    inline const juce::Colour text        { 0xfff2f2f2 };
    inline const juce::Colour textDim     { 0xff888888 };
    inline const juce::Colour textFaint   { 0xff5a5a5a };

    inline const juce::Colour accent      { 0xffc5f467 };   // acid green: active state, key signal, clipping
    inline const juce::Colour warn        { 0xffff6b4a };   // the only other colour: overload

    // Logical (unscaled) size of the editor. The whole interface is laid out on this grid and scaled
    // uniformly when the host window is resized.
    inline constexpr int designWidth  = 900;
    inline constexpr int designHeight = 560;
}
