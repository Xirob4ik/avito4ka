#pragma once

#include <cstddef>

namespace clipline::presets
{
    /** One factory preset, in the units shown to the user. Header-only data, no JUCE types. */
    struct PresetValues
    {
        const char* name;
        const char* category;
        float inputDb;
        float clipDb;       // dBFS
        float outputDb;
        float mixPercent;
        int   mode;         // 0 HARD, 1 SOFT, 2 CUBIC, 3 WAVEFOLD
        int   oversampling; // 0 1x, 1 2x, 2 4x, 3 8x
        bool  link;
        bool  ceiling = true;   // CLIP is a hard ceiling (see ClipperEngine)
    };

    /** Number of factory presets. */
    std::size_t getNumFactoryPresets() noexcept;

    /** @param index 0 .. getNumFactoryPresets() - 1 */
    const PresetValues& getFactoryPreset (std::size_t index) noexcept;

    /** Index of the preset with this exact name, or -1. */
    int findFactoryPreset (const char* name) noexcept;
}
