#include <juce_core/juce_core.h>

#include <thread>

#include "DSP/ClipperEngine.h"
#include "DSP/ClippingFunctions.h"
#include "DSP/DelayLine.h"
#include "DSP/Metering.h"
#include "DSP/WaveformTap.h"
#include "TestUtil.h"

using namespace clipline::dsp;

namespace
{
    constexpr std::array<ClipMode, 4> kAllModes { ClipMode::Hard, ClipMode::Soft, ClipMode::Cubic, ClipMode::Fold };

    const char* modeName (ClipMode m)
    {
        switch (m)
        {
            case ClipMode::Hard:  return "HARD";
            case ClipMode::Soft:  return "SOFT";
            case ClipMode::Cubic: return "CUBIC";
            case ClipMode::Fold:  return "FOLD";
        }
        return "?";
    }
}

//==================================================================================================
struct TransferFunctionTests : public juce::UnitTest
{
    TransferFunctionTests() : juce::UnitTest ("Transfer functions", "Clipline-DSP") {}

    void runTest() override
    {
        beginTest ("every mode is odd-symmetric (no DC is created for symmetric signals)");
        for (auto mode : kAllModes)
        {
            float worst = 0.0f;
            for (float u = 0.0f; u <= 12.0f; u += 0.0137f)
                worst = std::max (worst, std::abs (shape (mode, u) + shape (mode, -u)));
            expectLessThan (worst, 1.0e-5f, juce::String (modeName (mode)) + " odd symmetry");
        }

        beginTest ("every mode has unit slope at the origin");
        for (auto mode : kAllModes)
        {
            const float h = 1.0e-3f;
            const float slope = (shape (mode, h) - shape (mode, -h)) / (2.0f * h);
            expectWithinAbsoluteError (slope, 1.0f, 2.0e-3f, juce::String (modeName (mode)) + " f'(0)");
        }

        beginTest ("every mode is bounded by the threshold for any input");
        for (auto mode : kAllModes)
        {
            float worst = 0.0f;
            for (float u = -200.0f; u <= 200.0f; u += 0.0101f)
                worst = std::max (worst, std::abs (shape (mode, u)));
            expectLessThan (worst, 1.0f + 1.0e-6f, juce::String (modeName (mode)) + " bounded");
        }

        beginTest ("HARD is exactly clamp(u, -1, 1)");
        for (float u = -5.0f; u <= 5.0f; u += 0.01f)
            expectEquals (hardClip (u), std::min (1.0f, std::max (-1.0f, u)));

        beginTest ("SOFT is tanh and strictly monotonic");
        {
            float prev = softClip (-10.0f);
            bool monotonic = true;
            for (float u = -9.99f; u <= 10.0f; u += 0.01f)
            {
                const float y = softClip (u);
                monotonic = monotonic && (y >= prev);
                prev = y;
                expectWithinAbsoluteError (y, std::tanh (u), 1.0e-6f);
            }
            expect (monotonic, "tanh monotonic");
        }

        beginTest ("CUBIC: u - 4/27 u^3 up to 1.5, exactly 1 with zero slope at the saturation point");
        {
            expectWithinAbsoluteError (cubicClip (1.5f), 1.0f, 1.0e-6f);
            expectWithinAbsoluteError (cubicClip (1.5001f), 1.0f, 1.0e-6f);
            expectWithinAbsoluteError (cubicClip (-1.5f), -1.0f, 1.0e-6f);
            expectWithinAbsoluteError (cubicClip (1.0f), 1.0f - 4.0f / 27.0f, 1.0e-6f);

            const float h = 1.0e-3f;
            expectLessThan (std::abs ((cubicClip (1.5f) - cubicClip (1.5f - h)) / h), 2.0e-3f, "C1 continuity at 1.5");

            bool monotonic = true;
            float prev = 0.0f;
            for (float u = 0.0f; u <= 4.0f; u += 0.001f)
            {
                const float y = cubicClip (u);
                monotonic = monotonic && y >= prev - 1.0e-7f;
                prev = y;
            }
            expect (monotonic, "monotonic on [0, 4]");
        }

        beginTest ("FOLD: triangle wave with period 4, continuous, bounded");
        {
            expectWithinAbsoluteError (foldClip (0.5f), 0.5f, 1.0e-6f);
            expectWithinAbsoluteError (foldClip (1.0f), 1.0f, 1.0e-6f);
            expectWithinAbsoluteError (foldClip (1.5f), 0.5f, 1.0e-6f);    // reflected: 2 - 1.5
            expectWithinAbsoluteError (foldClip (2.0f), 0.0f, 1.0e-6f);
            expectWithinAbsoluteError (foldClip (3.0f), -1.0f, 1.0e-6f);
            expectWithinAbsoluteError (foldClip (4.0f), 0.0f, 1.0e-6f);

            float worstPeriod = 0.0f, worstJump = 0.0f;
            for (float u = -20.0f; u <= 20.0f; u += 0.0173f)
            {
                worstPeriod = std::max (worstPeriod, std::abs (foldClip (u + 4.0f) - foldClip (u)));
                worstJump   = std::max (worstJump,   std::abs (foldClip (u + 0.0173f) - foldClip (u)));
            }
            expectLessThan (worstPeriod, 2.0e-5f, "period 4");
            expectLessThan (worstJump, 0.0173f * 1.01f + 1.0e-5f, "slope magnitude is 1 everywhere (no jumps)");
        }

        beginTest ("the four modes are genuinely different functions");
        for (size_t a = 0; a < kAllModes.size(); ++a)
            for (size_t b = a + 1; b < kAllModes.size(); ++b)
            {
                int differing = 0, total = 0;
                for (float u = 0.0f; u <= 4.0f; u += 0.01f, ++total)
                    if (std::abs (shape (kAllModes[a], u) - shape (kAllModes[b], u)) > 1.0e-3f)
                        ++differing;
                expectGreaterThan (differing, total / 8,
                                   juce::String (modeName (kAllModes[a])) + " vs " + modeName (kAllModes[b]));
            }
    }
};
static TransferFunctionTests transferFunctionTests;

//==================================================================================================
struct ShaperKernelTests : public juce::UnitTest
{
    ShaperKernelTests() : juce::UnitTest ("applyClipper kernels", "Clipline-DSP") {}

    struct Block
    {
        std::vector<float> l, r, thr, inv;
        Block (int n, float threshold) : l ((size_t) n), r ((size_t) n), thr ((size_t) n, threshold), inv ((size_t) n, 1.0f / threshold) {}
    };

    void runTest() override
    {
        constexpr int N = 20000;
        juce::Random rng (42);

        beginTest ("HARD never exceeds the threshold and is bit-exact below it");
        for (float T : { 0.05f, 0.3f, 0.7079f, 1.0f })
            for (bool link : { false, true })
            {
                Block b (N, T);
                for (int i = 0; i < N; ++i) { b.l[(size_t) i] = (rng.nextFloat() * 2.0f - 1.0f) * 8.0f; b.r[(size_t) i] = (rng.nextFloat() * 2.0f - 1.0f) * 8.0f; }
                const auto l0 = b.l, r0 = b.r;
                float* ch[2] = { b.l.data(), b.r.data() };
                const float ratio = applyClipper (ch, 2, N, b.thr.data(), b.inv.data(), { ClipMode::Hard, link });

                float peak = 0.0f; bool exactBelow = true;
                for (int i = 0; i < N; ++i)
                {
                    peak = std::max ({ peak, std::abs (b.l[(size_t) i]), std::abs (b.r[(size_t) i]) });
                    if (std::max (std::abs (l0[(size_t) i]), std::abs (r0[(size_t) i])) <= T)
                        exactBelow = exactBelow && juce::exactlyEqual (b.l[(size_t) i], l0[(size_t) i]) && juce::exactlyEqual (b.r[(size_t) i], r0[(size_t) i]);
                }
                expect (peak <= T, "peak <= T (T=" + juce::String (T) + ", link=" + juce::String ((int) link) + ")");
                expect (exactBelow, "samples below T pass through bit-exactly");
                expectGreaterThan (ratio, 7.0f, "reported overshoot ratio is |x|max / T");
            }

        beginTest ("every mode, linked or not, is bounded by the threshold");
        for (auto mode : kAllModes)
            for (bool link : { false, true })
            {
                const float T = 0.3f;
                Block b (N, T);
                for (int i = 0; i < N; ++i) { b.l[(size_t) i] = (rng.nextFloat() * 2.0f - 1.0f) * 50.0f; b.r[(size_t) i] = (rng.nextFloat() * 2.0f - 1.0f) * 0.1f; }
                float* ch[2] = { b.l.data(), b.r.data() };
                applyClipper (ch, 2, N, b.thr.data(), b.inv.data(), { mode, link });
                float peak = 0.0f;
                for (int i = 0; i < N; ++i)
                    peak = std::max ({ peak, std::abs (b.l[(size_t) i]), std::abs (b.r[(size_t) i]) });
                expect (peak <= T, juce::String (modeName (mode)) + (link ? " linked" : " unlinked") + " |y| <= T");
            }

        beginTest ("LINK keeps the instantaneous L/R ratio (stereo image); unlinked does not");
        for (auto mode : { ClipMode::Hard, ClipMode::Soft, ClipMode::Cubic, ClipMode::Fold })
        {
            const float T = 0.5f;
            Block lin (N, T), unl (N, T);
            for (int i = 0; i < N; ++i)
            {
                const float s = std::sin ((float) i * 0.05f) * 3.0f;
                lin.l[(size_t) i] = unl.l[(size_t) i] = s;
                lin.r[(size_t) i] = unl.r[(size_t) i] = s * 0.35f;
            }
            const auto l0 = lin.l;
            float* a[2] = { lin.l.data(), lin.r.data() };
            float* c[2] = { unl.l.data(), unl.r.data() };
            applyClipper (a, 2, N, lin.thr.data(), lin.inv.data(), { mode, true });
            applyClipper (c, 2, N, unl.thr.data(), unl.inv.data(), { mode, false });

            // Skip samples where the output is near zero (FOLD crosses zero): the ratio is ill-conditioned there.
            float worstLinked = 0.0f, worstUnlinked = 0.0f;
            for (int i = 0; i < N; ++i)
            {
                if (std::abs (l0[(size_t) i]) < 0.1f) continue;
                if (std::abs (lin.l[(size_t) i]) > 0.02f)
                    worstLinked = std::max (worstLinked, std::abs (lin.r[(size_t) i] / lin.l[(size_t) i] - 0.35f));
                if (std::abs (unl.l[(size_t) i]) > 0.02f)
                    worstUnlinked = std::max (worstUnlinked, std::abs (unl.r[(size_t) i] / unl.l[(size_t) i] - 0.35f));
            }
            expectLessThan (worstLinked, 2.0e-3f, juce::String (modeName (mode)) + ": linked ratio preserved");
            expectGreaterThan (worstUnlinked, 0.05f, juce::String (modeName (mode)) + ": unlinked ratio changes when the louder channel clips");
        }

        beginTest ("mono is independent of LINK");
        for (auto mode : kAllModes)
        {
            Block a (N, 0.4f), b (N, 0.4f);
            for (int i = 0; i < N; ++i)
                a.l[(size_t) i] = b.l[(size_t) i] = (rng.nextFloat() * 2.0f - 1.0f) * 3.0f;
            float* pa[1] = { a.l.data() };
            float* pb[1] = { b.l.data() };
            applyClipper (pa, 1, N, a.thr.data(), a.inv.data(), { mode, true });
            applyClipper (pb, 1, N, b.thr.data(), b.inv.data(), { mode, false });
            expect (a.l == b.l, juce::String (modeName (mode)) + " mono identical for both LINK states");
        }

        beginTest ("overshoot ratio return value");
        {
            Block b (1000, 0.25f);
            for (int i = 0; i < 1000; ++i) { b.l[(size_t) i] = 0.1f; b.r[(size_t) i] = -0.1f; }
            b.l[500] = -0.9f;
            float* ch[2] = { b.l.data(), b.r.data() };
            expectWithinAbsoluteError (applyClipper (ch, 2, 1000, b.thr.data(), b.inv.data(), { ClipMode::Hard, false }), 3.6f, 1.0e-4f);
        }
    }
};
static ShaperKernelTests shaperKernelTests;

//==================================================================================================
struct SupportStructureTests : public juce::UnitTest
{
    SupportStructureTests() : juce::UnitTest ("Meters, tap and delay line", "Clipline-DSP") {}

    void runTest() override
    {
        beginTest ("MeterState keeps the maximum since the last read and clears on read");
        {
            MeterState m;
            m.pushInputPeak (0, 0.2f);  m.pushInputPeak (0, 0.9f);  m.pushInputPeak (0, 0.5f);
            m.pushInputPeak (1, 0.1f);
            m.pushOutputPeak (1, 0.4f);
            m.pushClipRatio (1.7f);     m.pushClipRatio (0.3f);
            expectEquals (m.takeInputPeak (0), 0.9f);
            expectEquals (m.takeInputPeak (0), 0.0f);
            expectEquals (m.takeInputPeak (1), 0.1f);
            expectEquals (m.takeOutputPeak (1), 0.4f);
            expectEquals (m.takeOutputPeak (0), 0.0f);
            expectEquals (m.takeClipRatio(), 1.7f);
            expectEquals (m.takeClipRatio(), 0.0f);
        }

        beginTest ("MeterState tolerates concurrent writers and readers (no lost maximum)");
        {
            MeterState m;
            std::atomic<bool> stop { false };
            std::thread writer ([&]
            {
                for (int i = 0; i < 200000; ++i)
                    m.pushInputPeak (0, (float) (i % 1000) * 0.001f);
                m.pushInputPeak (0, 5.0f);
                stop = true;
            });
            float seenMax = 0.0f;
            while (! stop)
                seenMax = std::max (seenMax, m.takeInputPeak (0));
            writer.join();
            seenMax = std::max (seenMax, m.takeInputPeak (0));
            expectEquals (seenMax, 5.0f);
        }

        beginTest ("MeterBallistics: instant attack, constant dB/s release, timed peak hold");
        {
            MeterBallistics b;
            b.setTimes (24.0f, 1.5f);
            b.reset();
            b.update (1.0f, 0.016f);
            expectWithinAbsoluteError (b.getLevelDb(), 0.0f, 1.0e-4f);
            expectWithinAbsoluteError (b.getHoldDb(), 0.0f, 1.0e-4f);

            for (int i = 0; i < 30; ++i) b.update (0.0f, 1.0f / 60.0f);    // 0.5 s of silence
            expectWithinAbsoluteError (b.getLevelDb(), -12.0f, 0.2f);       // 24 dB/s release
            expectWithinAbsoluteError (b.getHoldDb(), 0.0f, 1.0e-4f);       // still held (1.5 s)

            for (int i = 0; i < 90; ++i) b.update (0.0f, 1.0f / 60.0f);    // another 1.5 s -> hold expired, falling
            expect (b.getHoldDb() < -2.0f, "peak hold falls after the hold time");
            expectWithinAbsoluteError (b.getMaxDb(), 0.0f, 1.0e-4f);        // session maximum is retained
            b.resetHold();
            expect (b.getMaxDb() < -20.0f, "resetHold clears the session maximum");
        }

        beginTest ("WaveformTap condenses audio into min/max columns");
        {
            WaveformTap tap;
            tap.prepare (48000.0);                      // 3 samples per column
            std::vector<float> inMin (120), inMax (120), outMin (120), outMax (120);
            for (int i = 0; i < 120; ++i)
            {
                inMin[(size_t) i] = inMax[(size_t) i] = (float) i;
                outMin[(size_t) i] = outMax[(size_t) i] = -(float) i;
            }
            // Push in odd-sized pieces: column boundaries must not depend on block sizes.
            tap.push (inMin.data(),       inMax.data(),       outMin.data(),       outMax.data(),       37);
            tap.push (inMin.data() + 37,  inMax.data() + 37,  outMin.data() + 37,  outMax.data() + 37,  1);
            tap.push (inMin.data() + 38,  inMax.data() + 38,  outMin.data() + 38,  outMax.data() + 38,  82);

            std::array<WaveformTap::Column, 64> cols;
            std::uint64_t end = 0;
            const int n = tap.copyLatest (cols.data(), 64, end);
            expectEquals (n, 40);
            expectEquals ((int) end, 40);
            for (int c = 0; c < 40; ++c)
            {
                expectEquals (cols[(size_t) c].inMin, (float) (c * 3));
                expectEquals (cols[(size_t) c].inMax, (float) (c * 3 + 2));
                expectEquals (cols[(size_t) c].outMin, -(float) (c * 3 + 2));
                expectEquals (cols[(size_t) c].outMax, -(float) (c * 3));
            }
        }

        beginTest ("DelayLine: read(delay) and readHistory");
        {
            DelayLine d;
            d.prepare (2, 100, 64);
            std::vector<float> src (1000);
            for (int i = 0; i < 1000; ++i) src[(size_t) i] = (float) (i + 1);

            std::vector<float> out (64), hist (50);
            bool ok = true;
            int pos = 0;
            for (int blockSize : { 64, 1, 17, 33, 64, 64, 5 })
            {
                const float* s[2] = { src.data() + pos, src.data() + pos };

                if (pos >= 50)
                {
                    d.readHistory (0, hist.data(), 50);
                    for (int i = 0; i < 50; ++i)
                        ok = ok && juce::exactlyEqual (hist[(size_t) i], src[(size_t) (pos - 50 + i)]);
                }

                d.push (s, 2, blockSize);
                for (int delay : { 0, 1, 31, 100 })
                {
                    d.read (1, out.data(), blockSize, delay);
                    for (int i = 0; i < blockSize; ++i)
                    {
                        const int idx = pos + i - delay;
                        const float expected = idx >= 0 ? src[(size_t) idx] : 0.0f;
                        ok = ok && juce::exactlyEqual (out[(size_t) i], expected);
                    }
                }
                pos += blockSize;
            }
            expect (ok, "delayed reads match the pushed sequence");
        }
    }
};
static SupportStructureTests supportStructureTests;
