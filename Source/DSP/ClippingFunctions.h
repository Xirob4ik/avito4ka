#pragma once

#include <cmath>

namespace clipline::dsp
{
    /** The four clipping algorithms. The integer values are also the indices of the host-visible
        "mode" choice parameter - do not reorder. */
    enum class ClipMode : int
    {
        Hard = 0,
        Soft,
        Cubic,
        Fold
    };

    inline constexpr int kNumClipModes = 4;

    /*  All transfer functions work on the *normalised* amplitude u = x / T, where T is the linear
        clip threshold, and return y / T. Consequently every mode has:

          - odd symmetry:   f(-u) = -f(u)       (no DC offset is created for symmetric signals)
          - unity slope at the origin: f'(0) = 1 (signals far below the threshold pass unchanged)
          - |f(u)| <= 1     (the output never exceeds the threshold T)
    */

    /** HARD: f(u) = clamp(u, -1, 1). Exact: |y| <= T for any input. */
    inline float hardClip (float u) noexcept
    {
        return u > 1.0f ? 1.0f : (u < -1.0f ? -1.0f : u);
    }

    /** SOFT: f(u) = tanh(u). Smooth, strictly monotonic, approaches +-1 asymptotically. */
    inline float softClip (float u) noexcept
    {
        return std::tanh (u);
    }

    /** CUBIC: odd cubic saturator with a bounded working region |u| <= 1.5.

            f(u) = u - (4/27) u^3        for |u| <= 1.5
            f(u) = sign(u)               for |u| >  1.5

        Derivation: f(u) = u - u^3 / (3 s^2) has f'(s) = 0 (a flat, C1-continuous saturation point)
        and f(s) = 2s/3. Requiring f(s) = 1 gives s = 3/2, hence the coefficient 1/(3*s^2) = 4/27.
        The curve reaches exactly the threshold at 1.5 T (+3.5 dB over it) with zero slope. */
    inline float cubicClip (float u) noexcept
    {
        constexpr float kSaturationPoint = 1.5f;
        constexpr float kCoefficient     = 4.0f / 27.0f;

        if (u >= kSaturationPoint)  return 1.0f;
        if (u <= -kSaturationPoint) return -1.0f;
        return u - kCoefficient * u * u * u;
    }

    /** FOLD: triangular wavefolder with period 4 (reflecting at +-1).

            f(u) = u                    for |u| <= 1
            f(u) = 2 - u                for 1 < u <= 3      (reflected back from +1)
            f(u) = -2 - u               for -3 <= u < -1    (reflected back from -1)
            ... continuing periodically.

        Implemented as p = u - 4*round(u/4) in [-2, 2], then f = p if |p| <= 1 else copysign(2,p) - p.
        Always bounded to [-1, 1]. */
    inline float foldClip (float u) noexcept
    {
        const float p = u - 4.0f * std::floor (u * 0.25f + 0.5f);   // p in [-2, 2)
        const float a = std::abs (p);
        return a <= 1.0f ? p : std::copysign (2.0f, p) - p;
    }

    /** Dispatches to the selected algorithm. */
    inline float shape (ClipMode mode, float u) noexcept
    {
        switch (mode)
        {
            case ClipMode::Hard:  return hardClip (u);
            case ClipMode::Soft:  return softClip (u);
            case ClipMode::Cubic: return cubicClip (u);
            case ClipMode::Fold:  return foldClip (u);
        }
        return hardClip (u);
    }
}
