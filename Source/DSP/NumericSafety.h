#pragma once

#include <cstdint>
#include <cstring>

namespace clipline::dsp
{
    /** True when the float is neither NaN nor +-infinity.

        Implemented with bit inspection rather than std::isfinite so it keeps working even if a
        build (or a host-side flag) enables -ffast-math / /fp:fast, where the compiler is allowed
        to assume NaN/Inf never occur and may optimise std::isfinite away. */
    inline bool isFiniteBits (float x) noexcept
    {
        std::uint32_t bits;
        std::memcpy (&bits, &x, sizeof (bits));
        return (bits & 0x7F800000u) != 0x7F800000u;
    }

    /** Replaces NaN / +-Inf with 0. */
    inline float sanitize (float x) noexcept
    {
        return isFiniteBits (x) ? x : 0.0f;
    }
}
