#include "FactoryPresets.h"

#include <cstring>

namespace clipline::presets
{
    namespace
    {
        // Reading the numbers: CLIP is the ceiling of the internal clipper. OUTPUT is applied after
        // clipping, so with MIX at 100% the nominal peak level is CLIP + OUTPUT. Every preset keeps that sum
        // at least 1.4 dB below full scale, because the anti-alias filters of the oversampled modes let the
        // filtered result ring above the ceiling: ~0 dB for content below 8 kHz, up to ~1.6 dB for clipped
        // tones above 10 kHz and ~3 dB for full-scale broadband noise (docs/ALGORITHMS.md). The margin is
        // enough for drum, bass and mix material - a unit test measures every preset on such material - but
        // the plugin is not a true-peak limiter: extreme, noise-like input can still pass the nominal ceiling.
        //
        //                                 INPUT   CLIP   OUTPUT  MIX   mode  os  link
        constexpr PresetValues kPresets[] =
        {
            { "Init",                 "Utility",   0.0f, -3.0f,  0.0f, 100.0f, 0, 2, true  },

            // Peak control: a little clipping on transients only.
            { "Clean Peak Control",   "Control",  +1.5f, -2.0f, +0.5f, 100.0f, 0, 2, true  },
            { "Mix Bus Clipper",      "Control",  +2.5f, -2.0f, +0.5f, 100.0f, 0, 3, true  },

            // Drums.
            { "Punchy Drums",         "Drums",    +4.0f, -4.0f, +1.0f, 100.0f, 0, 2, true  },
            { "Aggressive Drums",     "Drums",   +10.0f, -6.0f, +3.0f, 100.0f, 0, 2, false },

            // Low end: smooth shaping instead of a hard corner keeps the body of the bass.
            { "808 Control",          "Bass",     +5.0f, -3.0f, +1.5f, 100.0f, 1, 2, true  },

            // Colour.
            { "Soft Saturation",      "Colour",   +8.0f, -8.0f, +2.0f, 100.0f, 1, 2, true  },
            { "Cubic Warmth",         "Colour",   +6.0f, -4.0f, +1.0f,  80.0f, 2, 2, true  },

            // Extreme.
            { "Hyperpop Smash",       "Extreme", +18.0f, -6.0f, +3.0f, 100.0f, 0, 3, false },
            { "Wavefold Texture",     "Extreme",  +8.0f, -6.0f,  0.0f,  60.0f, 3, 3, false },
        };

        constexpr std::size_t kCount = sizeof (kPresets) / sizeof (kPresets[0]);
    }

    std::size_t getNumFactoryPresets() noexcept
    {
        return kCount;
    }

    const PresetValues& getFactoryPreset (std::size_t index) noexcept
    {
        return kPresets[index < kCount ? index : 0];
    }

    int findFactoryPreset (const char* name) noexcept
    {
        for (std::size_t i = 0; i < kCount; ++i)
            if (std::strcmp (kPresets[i].name, name) == 0)
                return (int) i;
        return -1;
    }
}
