#include <juce_core/juce_core.h>

#include <chrono>
#include <iostream>

#include "DSP/ClipperEngine.h"
#include "DSP/OversamplingProcessor.h"
#include "TestUtil.h"

using namespace cltest;

namespace
{
    constexpr double kFs = 48000.0;
    constexpr std::array<ClipMode, 4> kModes { ClipMode::Hard, ClipMode::Soft, ClipMode::Cubic, ClipMode::Fold };
    const char* kModeNames[] = { "HARD", "SOFT", "CUBIC", "FOLD" };
    const char* kOsNames[]   = { "1x", "2x", "4x", "8x" };

    EngineSettings base()
    {
        EngineSettings s;
        s.inputDb = 0.0f; s.clipDb = -3.0f; s.outputDb = 0.0f; s.mixPercent = 100.0f;
        s.mode = ClipMode::Hard; s.oversamplingIndex = 0; s.link = true;
        return s;
    }

    float tLin (float clipDb) { return ClipperEngine::dbToGain (clipDb); }

    /** Renders [0, switchAt) with `a` and [switchAt, end) with `b` through one engine (block size `blockSize`). */
    juce::AudioBuffer<float> renderWithChange (const juce::AudioBuffer<float>& input, const EngineSettings& a,
                                               const EngineSettings& b, int switchAt, int blockSize, int maxBlock = 512)
    {
        ClipperEngine engine;
        engine.prepare (kFs, maxBlock, input.getNumChannels());

        juce::AudioBuffer<float> out (input);
        int pos = 0;
        while (pos < out.getNumSamples())
        {
            const auto& s = pos < switchAt ? a : b;
            const int limit = pos < switchAt ? switchAt : out.getNumSamples();
            const int n = std::min (blockSize, limit - pos);
            juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), out.getNumChannels(), pos, n);
            engine.process (view, s);
            pos += n;
        }
        return out;
    }
}

//==================================================================================================
struct LatencyTests : public juce::UnitTest
{
    LatencyTests() : juce::UnitTest ("Oversampling latency and dry/wet alignment", "Clipline-Engine") {}

    void runTest() override
    {
        beginTest ("every oversampling chain has an exact integer latency (design table is valid)");
        for (int stages = 1; stages <= OversamplingProcessor::kNumStages; ++stages)
            expectEquals (OversamplingProcessor::designLatencyEighths (OversamplingProcessor::stageDesigns(), stages) % 8, 0,
                          juce::String (1 << stages) + "x latency must be a whole number of samples");

        beginTest ("the impulse response peaks exactly at the reported latency");
        {
            juce::String report = "latency [samples]:";
            for (int idx = 0; idx < 4; ++idx)
            {
                ClipperEngine engine;
                engine.prepare (kFs, 512, 1);
                auto s = base();
                s.clipDb = 0.0f;
                s.oversamplingIndex = idx;

                juce::AudioBuffer<float> in (1, 1024);
                in.clear();
                in.setSample (0, 100, 0.1f);
                const auto out = render (engine, in, s, 128);

                int argmax = 0;
                for (int i = 0; i < 1024; ++i)
                    if (std::abs (out.getSample (0, i)) > std::abs (out.getSample (0, argmax)))
                        argmax = i;

                expectEquals (argmax - 100, engine.getLatencySamples(), juce::String (kOsNames[idx]) + ": peak position == latency");
                expectEquals (OversamplingProcessor::designLatencyEighths (OversamplingProcessor::stageDesigns(), idx), 8 * engine.getLatencySamples());
                report << "  " << kOsNames[idx] << " = " << engine.getLatencySamples();
            }
            logMessage (report);
        }

        beginTest ("MIX = 0 returns the input delayed by exactly the latency (bit-exact)");
        for (int idx = 0; idx < 4; ++idx)
        {
            auto s = base();
            s.mixPercent = 0.0f;
            s.oversamplingIndex = idx;
            const auto in = makeNoise (2, 5000, 0.8f);
            ClipperEngine engine;
            engine.prepare (kFs, 512, 2);
            const auto out = render (engine, in, s, 100);
            const int L = engine.getLatencySamples();

            bool exact = true;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 5000; ++i)
                    exact = exact && juce::exactlyEqual (out.getSample (ch, i), (i >= L ? in.getSample (ch, i - L) : 0.0f));
            expect (exact, juce::String (kOsNames[idx]) + ": dry path delayed by the latency");
        }

        beginTest ("below the threshold the wet path equals the delayed input (wet and dry are aligned at every MIX)");
        for (int idx = 1; idx < 4; ++idx)
            for (double f : { 1000.0, 10000.0 })
                for (float mix : { 100.0f, 50.0f })
                {
                    auto s = base();
                    s.clipDb = 0.0f;
                    s.mixPercent = mix;
                    s.oversamplingIndex = idx;
                    const auto in = makeSine (1, 12000, kFs, f, 0.25f);
                    ClipperEngine engine;
                    engine.prepare (kFs, 512, 1);
                    const auto out = render (engine, in, s, 256);
                    const int L = engine.getLatencySamples();

                    double err = 0.0;
                    const int count = 6000;
                    for (int i = 3000; i < 3000 + count; ++i)
                    {
                        const double d = (double) out.getSample (0, i) - (double) in.getSample (0, i - L);
                        err += d * d;
                    }
                    const double rms = std::sqrt (err / count);
                    logMessage (juce::String (kOsNames[idx]) + " " + juce::String (f, 0) + " Hz mix " + juce::String (mix, 0)
                                + "%: error vs delayed input = " + juce::String (ampToDb (rms / 0.25 * std::sqrt (2.0)), 1) + " dB re signal");
                    expectLessThan (rms, 3.0e-4, juce::String (kOsNames[idx]) + " " + juce::String (f, 0) + " Hz, mix " + juce::String (mix, 0));
                }
    }
};
static LatencyTests latencyTests;

//==================================================================================================
struct ClippingBehaviourTests : public juce::UnitTest
{
    ClippingBehaviourTests() : juce::UnitTest ("Clipping, gain staging and mix", "Clipline-Engine") {}

    void runTest() override
    {
        beginTest ("HARD at 1x: output never exceeds the threshold and reaches it");
        for (float clipDb : { -24.0f, -12.0f, -6.0f, -3.0f, -0.1f, 0.0f })
        {
            auto s = base();
            s.clipDb = clipDb;
            const auto out = renderFresh (makeSine (2, 20000, kFs, 997.0, 2.0f), s);
            const float T = tLin (clipDb);
            for (int ch = 0; ch < 2; ++ch)
            {
                const float pk = peakOf (out, ch);
                expect (pk <= T, "peak <= T at " + juce::String (clipDb) + " dB");
                expectGreaterThan (pk, T * 0.9999f, "peak reaches T at " + juce::String (clipDb) + " dB");
            }
        }

        beginTest ("order: INPUT before the clipper, OUTPUT after it (final peak = T * OUTPUT gain)");
        {
            auto s = base();
            s.clipDb = -3.0f;          // T = 0.708
            s.inputDb = 6.0f;          // 0.5 -> 0.998 (clips)
            s.outputDb = 4.0f;
            const auto out = renderFresh (makeSine (1, 20000, kFs, 997.0, 0.5f), s);
            const float expected = tLin (-3.0f) * ClipperEngine::dbToGain (4.0f);
            expectWithinAbsoluteError (peakOf (out, 0), expected, expected * 1.0e-5f);
            expectGreaterThan (peakOf (out, 0), tLin (-3.0f), "OUTPUT may raise the final level above the clip threshold");

            // INPUT really acts before the clipper: a signal that is below T without INPUT gain is clipped with it.
            auto quiet = base();  quiet.clipDb = -6.0f;
            auto driven = quiet;  driven.inputDb = 12.0f;
            const auto a = renderFresh (makeSine (1, 5000, kFs, 997.0, 0.3f), quiet);
            const auto b = renderFresh (makeSine (1, 5000, kFs, 997.0, 0.3f), driven);
            expectLessThan (peakOf (a, 0), 0.3001f, "0.3 < T(-6 dB) -> untouched");
            expectWithinAbsoluteError (peakOf (b, 0), tLin (-6.0f), 1.0e-6f);
        }

        beginTest ("INPUT and OUTPUT are plain gains when nothing clips");
        for (int idx : { 0, 2 })
        {
            auto s = base();
            s.clipDb = 0.0f; s.inputDb = 12.0f; s.outputDb = -6.0f; s.oversamplingIndex = idx;
            const auto in = makeSine (1, 12000, kFs, 1000.0, 0.05f);
            ClipperEngine engine;
            engine.prepare (kFs, 512, 1);
            const auto out = render (engine, in, s, 256);
            const int L = engine.getLatencySamples();
            const float g = ClipperEngine::dbToGain (12.0f) * ClipperEngine::dbToGain (-6.0f);
            double worst = 0.0;
            for (int i = 4000; i < 12000; ++i)
                worst = std::max (worst, (double) std::abs (out.getSample (0, i) - in.getSample (0, i - L) * g));
            expectLessThan (worst, 3.0e-4 * 0.05 * g + 1.0e-6, juce::String (kOsNames[idx]) + ": y = x * gain");
        }

        beginTest ("MIX crossfades dry and wet linearly: y = (1-m) dry + m wet");
        for (int idx : { 0, 2 })
        {
            auto s = base();
            s.clipDb = -9.0f; s.inputDb = 6.0f; s.oversamplingIndex = idx; s.mode = ClipMode::Soft;
            auto noise = makeNoise (2, 8000, 0.6f);
            const auto sine = makeSine (2, 8000, kFs, 313.0, 0.7f);
            for (int ch = 0; ch < 2; ++ch) noise.addFrom (ch, 0, sine, ch, 0, 8000);

            s.mixPercent = 0.0f;   const auto y0   = renderFresh (noise, s);
            s.mixPercent = 100.0f; const auto y100 = renderFresh (noise, s);
            for (float m : { 0.25f, 0.5f, 0.8f })
            {
                s.mixPercent = m * 100.0f;
                const auto ym = renderFresh (noise, s);
                float worst = 0.0f;
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 8000; ++i)
                        worst = std::max (worst, std::abs (ym.getSample (ch, i) - ((1.0f - m) * y0.getSample (ch, i) + m * y100.getSample (ch, i))));
                expectLessThan (worst, 2.0e-6f, juce::String (kOsNames[idx]) + " mix " + juce::String (m));
            }
            expect (maxAbsDiff (y0, y100, 0, 0, 8000) > 0.1f, "dry and wet really differ");
        }

        beginTest ("oversampled hard clip: CEILING off lets the filters overshoot slightly, CEILING on holds the threshold exactly");
        {
            juce::String report = "output peak vs threshold on a heavily clipped 1 kHz sine (T = -6 dBFS):";
            for (bool ceiling : { false, true })
            {
                report << (ceiling ? "\n    CEILING on: " : "\n    CEILING off:");
                for (int idx = 0; idx < 4; ++idx)
                {
                    auto s = base();
                    s.clipDb = -6.0f; s.oversamplingIndex = idx; s.ceiling = ceiling;
                    const auto out = renderFresh (makeSine (1, 24000, kFs, 1000.0, 8.0f), s);
                    const double overshootDb = ampToDb ((double) peakOf (out, 0, 4000) / (double) tLin (-6.0f));
                    report << "  " << kOsNames[idx] << " " << juce::String (overshootDb, 2) << " dB";
                    if (idx == 0 || ceiling)
                        expectLessThan (overshootDb, 1.0e-4, juce::String (kOsNames[idx]) + (ceiling ? " with CEILING" : "") + " is exact");
                    else
                        expectLessThan (overshootDb, 1.5, juce::String (kOsNames[idx]) + " overshoot stays within the anti-alias filter ripple");
                    expectGreaterThan (overshootDb, -0.5, "still limits at the threshold");
                }
            }
            logMessage (report);
        }
    }
};
static ClippingBehaviourTests clippingBehaviourTests;

//==================================================================================================
struct ChannelTests : public juce::UnitTest
{
    ChannelTests() : juce::UnitTest ("Mono, stereo and LINK", "Clipline-Engine") {}

    void runTest() override
    {
        beginTest ("stereo with identical channels equals mono, for every mode and LINK state");
        for (int m = 0; m < 4; ++m)
            for (bool link : { false, true })
            {
                auto s = base();
                s.mode = kModes[(size_t) m]; s.link = link; s.clipDb = -6.0f; s.inputDb = 9.0f; s.oversamplingIndex = 2; s.mixPercent = 80.0f;
                const auto mono = makeNoise (1, 6000, 0.7f, 7);
                juce::AudioBuffer<float> stereo (2, 6000);
                stereo.copyFrom (0, 0, mono, 0, 0, 6000);
                stereo.copyFrom (1, 0, mono, 0, 0, 6000);

                const auto a = renderFresh (mono, s);
                const auto b = renderFresh (stereo, s);

                // Unlinked: literally the same arithmetic per channel -> bit-exact. Linked: g = f(u)/u is
                // mathematically identical to T f(x/T) for L == R but rounds differently (about 1 ulp).
                const float tolerance = link ? 1.0e-6f : 0.0f;
                const juce::String label = juce::String (kModeNames[m]) + (link ? " linked" : " unlinked");
                expectLessOrEqual (maxAbsDiff (a, b, 0, 0, 6000), tolerance, label + " L equals mono");
                float rdiff = 0.0f;
                for (int i = 0; i < 6000; ++i) rdiff = std::max (rdiff, std::abs (a.getSample (0, i) - b.getSample (1, i)));
                expectLessOrEqual (rdiff, tolerance, label + " R equals mono");
                expectEquals (maxAbsDiff (b, b, 0, 0, 6000), 0.0f);
                float lr = 0.0f;
                for (int i = 0; i < 6000; ++i) lr = std::max (lr, std::abs (b.getSample (0, i) - b.getSample (1, i)));
                expectEquals (lr, 0.0f, label + ": identical input channels give identical output channels");
            }

        beginTest ("LINK off: channels are processed independently");
        for (int m = 0; m < 4; ++m)
        {
            auto s = base();
            s.mode = kModes[(size_t) m]; s.link = false; s.clipDb = -9.0f; s.inputDb = 6.0f; s.oversamplingIndex = 1;
            auto left  = makeNoise (1, 6000, 0.9f, 11);
            auto right = makeNoise (1, 6000, 0.05f, 12);
            juce::AudioBuffer<float> stereo (2, 6000);
            stereo.copyFrom (0, 0, left, 0, 0, 6000);
            stereo.copyFrom (1, 0, right, 0, 0, 6000);

            const auto st = renderFresh (stereo, s);
            const auto l = renderFresh (left, s);
            const auto r = renderFresh (right, s);
            float dl = 0.0f, dr = 0.0f;
            for (int i = 0; i < 6000; ++i)
            {
                dl = std::max (dl, std::abs (st.getSample (0, i) - l.getSample (0, i)));
                dr = std::max (dr, std::abs (st.getSample (1, i) - r.getSample (0, i)));
            }
            expectEquals (dl, 0.0f, juce::String (kModeNames[m]) + " L independent of R");
            expectEquals (dr, 0.0f, juce::String (kModeNames[m]) + " R independent of L");
        }

        beginTest ("LINK on: a loud left channel pulls the right channel down by the same gain (image is preserved)");
        {
            auto s = base();
            s.clipDb = -12.0f; s.oversamplingIndex = 0; s.link = true;
            juce::AudioBuffer<float> in (2, 4000);
            for (int i = 0; i < 4000; ++i)
            {
                const float v = std::sin ((float) i * 0.07f);
                in.setSample (0, i, v);
                in.setSample (1, i, 0.3f * v);
            }
            const auto linked = renderFresh (in, s);
            s.link = false;
            const auto unlinked = renderFresh (in, s);

            float worstLinked = 0.0f, worstUnlinked = 0.0f;
            for (int i = 0; i < 4000; ++i)
            {
                if (std::abs (in.getSample (0, i)) < 0.3f) continue;
                worstLinked   = std::max (worstLinked,   std::abs (linked.getSample (1, i) / linked.getSample (0, i) - 0.3f));
                worstUnlinked = std::max (worstUnlinked, std::abs (unlinked.getSample (1, i) / unlinked.getSample (0, i) - 0.3f));
            }
            expectLessThan (worstLinked, 1.0e-4f, "linked: R/L stays 0.3");
            expectGreaterThan (worstUnlinked, 0.2f, "unlinked: R/L changes while L is clipped");
        }
    }
};
static ChannelTests channelTests;

//==================================================================================================
struct RobustnessTests : public juce::UnitTest
{
    RobustnessTests() : juce::UnitTest ("DC, silence, NaN/Inf and extreme levels", "Clipline-Engine") {}

    void runTest() override
    {
        beginTest ("silence in -> exact silence out (no DC, no noise) for every mode / oversampling / LINK");
        for (int m = 0; m < 4; ++m)
            for (int idx = 0; idx < 4; ++idx)
                for (bool link : { false, true })
                {
                    auto s = base();
                    s.mode = kModes[(size_t) m]; s.oversamplingIndex = idx; s.link = link; s.inputDb = 24.0f; s.outputDb = 24.0f;
                    juce::AudioBuffer<float> zeros (2, 4096);
                    zeros.clear();
                    const auto out = renderFresh (zeros, s);
                    expectEquals (peakOf (out, 0) + peakOf (out, 1), 0.0f,
                                  juce::String (kModeNames[m]) + " " + kOsNames[idx] + (link ? " linked" : " unlinked"));
                }

        beginTest ("a symmetric signal gets no DC offset in any mode");
        for (int m = 0; m < 4; ++m)
            for (int idx = 0; idx < 4; ++idx)
            {
                auto s = base();
                s.mode = kModes[(size_t) m]; s.oversamplingIndex = idx; s.clipDb = -6.0f; s.inputDb = 6.0f;
                const auto out = renderFresh (makeSine (1, 48000, kFs, 1000.0, 1.3f), s);
                const double mean = meanOf (out, 0, 4800, 48 * 500);           // whole number of cycles
                expectLessThan (std::abs (mean), 1.0e-5, juce::String (kModeNames[m]) + " " + kOsNames[idx] + " mean");
            }

        beginTest ("NaN, +-Inf and huge values never leak to the output and the engine recovers");
        for (int m = 0; m < 4; ++m)
            for (int idx = 0; idx < 4; ++idx)
            {
                auto s = base();
                s.mode = kModes[(size_t) m]; s.oversamplingIndex = idx; s.clipDb = 0.0f; s.inputDb = 12.0f; s.mixPercent = 70.0f;
                const auto clean = makeNoise (2, 9000, 0.4f, 21);
                auto dirty = clean;
                const float nan = std::numeric_limits<float>::quiet_NaN(), inf = std::numeric_limits<float>::infinity();
                dirty.setSample (0, 1000, nan);  dirty.setSample (1, 1500, nan);
                dirty.setSample (0, 2000, inf);  dirty.setSample (1, 2500, -inf);
                dirty.setSample (0, 3000, 1.0e30f);  dirty.setSample (1, 3500, -1.0e30f);
                dirty.setSample (0, 4000, 3.0e38f);

                const auto outDirty = renderFresh (dirty, s, kFs, 250);
                const auto outClean = renderFresh (clean, s, kFs, 250);
                expect (allFinite (outDirty), juce::String (kModeNames[m]) + " " + kOsNames[idx] + ": output finite");
                // 4000 is the last disturbed sample; the FIR memory is far shorter than 1000 samples.
                for (int ch = 0; ch < 2; ++ch)
                    expectLessThan (maxAbsDiff (outDirty, outClean, ch, 5200, 3800), 1.0e-5f,
                                    juce::String (kModeNames[m]) + " " + kOsNames[idx] + ": recovers after the burst");
            }

        beginTest ("extreme levels: -400 dB noise, denormals and +120 dB inputs stay finite and bounded");
        for (int idx = 0; idx < 4; ++idx)
            for (int m = 0; m < 4; ++m)
            {
                auto s = base();
                s.mode = kModes[(size_t) m]; s.oversamplingIndex = idx; s.clipDb = 0.0f;
                for (float amp : { 1.0e-30f, 1.0e-40f, 1.0e6f })
                {
                    const auto out = renderFresh (makeNoise (2, 6000, amp, 5), s);
                    expect (allFinite (out), juce::String (kModeNames[m]) + " " + kOsNames[idx] + " amp " + juce::String (amp));
                    expectLessThan (peakOf (out, 0), 2.0f, "bounded by the threshold (plus filter ripple)");
                }
            }
    }
};
static RobustnessTests robustnessTests;

//==================================================================================================
struct TransitionTests : public juce::UnitTest
{
    TransitionTests() : juce::UnitTest ("Click-free parameter and mode changes", "Clipline-Engine") {}

    void runTest() override
    {
        constexpr int N = 24000, switchAt = 9013;           // deliberately not a block multiple

        beginTest ("switching oversampling modes: no click, and the new chain is exactly in sync afterwards");
        {
            juce::String report;
            for (int from = 0; from < 4; ++from)
                for (int to = 0; to < 4; ++to)
                {
                    if (from == to) continue;
                    auto a = base();  a.clipDb = -6.0f; a.oversamplingIndex = from;
                    auto b = a;       b.oversamplingIndex = to;
                    const auto in = makeSine (1, N, kFs, 440.0, 1.0f);

                    const auto switched = renderWithChange (in, a, b, switchAt, 128);
                    const auto refA = renderFresh (in, a);
                    const auto refB = renderFresh (in, b);

                    const float baseline = std::max (maxStep (refA, 0, 4000, N), maxStep (refB, 0, 4000, N));
                    const float step = maxStep (switched, 0, switchAt - 300, N);
                    expectLessThan (step, baseline * 1.35f + 1.0e-4f,
                                    juce::String (kOsNames[from]) + " -> " + kOsNames[to] + ": largest sample step");

                    // After the crossfade (5 ms) and filter memory, the switched engine must equal one that
                    // ran the new mode all along: proves the warm-up from the dry history is exact.
                    expectLessThan (maxAbsDiff (switched, refB, 0, switchAt + 800, N - switchAt - 800), 1.0e-5f,
                                    juce::String (kOsNames[from]) + " -> " + kOsNames[to] + ": converges to the new mode");
                    report << kOsNames[from] << ">" << kOsNames[to] << " " << juce::String (step / baseline, 2) << "  ";
                }
            logMessage ("step / baseline: " + report);
        }

        beginTest ("switching the clipping mode: no click, converges to the new mode");
        for (int from = 0; from < 4; ++from)
            for (int to = 0; to < 4; ++to)
            {
                if (from == to) continue;
                for (int idx : { 0, 2 })
                {
                    auto a = base();  a.clipDb = -6.0f; a.oversamplingIndex = idx; a.mode = kModes[(size_t) from];
                    auto b = a;       b.mode = kModes[(size_t) to];
                    const auto in = makeSine (1, N, kFs, 440.0, 1.2f);

                    const auto switched = renderWithChange (in, a, b, switchAt, 100);
                    const auto refA = renderFresh (in, a);
                    const auto refB = renderFresh (in, b);

                    const float baseline = std::max (maxStep (refA, 0, 4000, N), maxStep (refB, 0, 4000, N));
                    const float step = maxStep (switched, 0, switchAt - 300, N);
                    expectLessThan (step, baseline * 1.35f + 1.0e-4f,
                                    juce::String (kModeNames[from]) + " -> " + kModeNames[to] + " @" + kOsNames[idx]);
                    expectLessThan (maxAbsDiff (switched, refB, 0, switchAt + 800, N - switchAt - 800), 1.0e-5f,
                                    juce::String (kModeNames[from]) + " -> " + kModeNames[to] + ": converges");
                }
            }

        beginTest ("toggling LINK: no click");
        {
            juce::AudioBuffer<float> in (2, N);
            for (int i = 0; i < N; ++i)
            {
                const float v = std::sin (2.0f * (float) kPi * 440.0f * (float) i / (float) kFs);
                in.setSample (0, i, 1.0f * v);
                in.setSample (1, i, 0.4f * v);
            }
            for (bool from : { false, true })
            {
                auto a = base(); a.clipDb = -9.0f; a.link = from; a.oversamplingIndex = 1;
                auto b = a;      b.link = ! from;
                const auto switched = renderWithChange (in, a, b, switchAt, 64);
                const auto refA = renderFresh (in, a);
                const auto refB = renderFresh (in, b);
                for (int ch = 0; ch < 2; ++ch)
                {
                    const float baseline = std::max (maxStep (refA, ch, 4000, N), maxStep (refB, ch, 4000, N));
                    expectLessThan (maxStep (switched, ch, switchAt - 300, N), baseline * 1.35f + 1.0e-4f, "LINK toggle ch " + juce::String (ch));
                }
            }
        }

        beginTest ("abrupt INPUT / OUTPUT / CLIP / MIX changes are smoothed (no zipper steps)");
        {
            const auto in = makeSine (1, N, kFs, 1000.0, 0.2f);
            const float slope = 0.2f * 2.0f * (float) kPi * 1000.0f / (float) kFs;

            auto a = base(); a.clipDb = 0.0f; a.inputDb = -24.0f;
            auto b = a;      b.inputDb = 12.0f;
            expectLessThan (maxStep (renderWithChange (in, a, b, switchAt, 100), 0, 0, N), slope * ClipperEngine::dbToGain (12.0f) * 1.05f, "INPUT jump");

            a = base(); a.clipDb = 0.0f; a.outputDb = 12.0f;
            b = a;      b.outputDb = -24.0f;
            expectLessThan (maxStep (renderWithChange (in, a, b, switchAt, 100), 0, 0, N), slope * ClipperEngine::dbToGain (12.0f) * 1.05f, "OUTPUT jump");

            const auto loud = makeSine (1, N, kFs, 1000.0, 1.0f);
            a = base(); a.clipDb = 0.0f;
            b = a;      b.clipDb = -24.0f;
            expectLessThan (maxStep (renderWithChange (loud, a, b, switchAt, 100), 0, 0, N), 1.0f * 2.0f * (float) kPi * 1000.0f / (float) kFs * 1.05f, "CLIP jump");

            a = base(); a.clipDb = -12.0f; a.mixPercent = 0.0f;
            b = a;      b.mixPercent = 100.0f;
            expectLessThan (maxStep (renderWithChange (loud, a, b, switchAt, 100), 0, 0, N), 1.0f * 2.0f * (float) kPi * 1000.0f / (float) kFs * 1.05f, "MIX jump");
        }

        beginTest ("results do not depend on the host's block size (including blocks larger than prepared)");
        {
            auto s = base();
            s.mode = ClipMode::Soft; s.oversamplingIndex = 2; s.inputDb = 6.0f; s.clipDb = -6.0f; s.mixPercent = 70.0f; s.outputDb = 1.0f;
            const auto in = makeNoise (2, 20000, 0.7f, 99);
            const auto reference = renderFresh (in, s, kFs, 512, 512);
            for (int blockSize : { 1, 7, 64, 333, 4096, 20000 })
            {
                const auto out = renderFresh (in, s, kFs, blockSize, 512);
                for (int ch = 0; ch < 2; ++ch)
                    expectLessThan (maxAbsDiff (out, reference, ch, 0, 20000), 1.0e-5f, "block size " + juce::String (blockSize));
            }
        }
    }
};
static TransitionTests transitionTests;

//==================================================================================================
struct AliasingTests : public juce::UnitTest
{
    AliasingTests() : juce::UnitTest ("Aliasing reduction", "Clipline-Engine") {}

    void runTest() override
    {
        // Worst case for aliasing: a 5 kHz sine driven 12 dB into the clipper, so nearly all of its energy is in
        // harmonics. Measured twice: with CEILING off this is the quality of the oversampling itself; with CEILING
        // on the base-rate safety clamp flattens the filter overshoot again and gives part of the gain back.
        constexpr int N = 16384;
        constexpr int fundamentalBin = 1700;                     // 4.98 kHz at 48 kHz: coherent in an N-point window
        const double f0 = (double) fundamentalBin * kFs / (double) N;

        auto measure = [&] (bool ceiling, double (&aliasDb)[4])
        {
            for (int idx = 0; idx < 4; ++idx)
            {
                auto s = base();
                s.clipDb = -6.0f; s.oversamplingIndex = idx; s.ceiling = ceiling;
                const int total = 3 * N;
                const auto out = renderFresh (makeSine (1, total, kFs, f0, 4.0f), s, kFs, 512, 512);
                const auto p = powerSpectrum (out.getReadPointer (0) + (total - N), N);

                // Only the audible band (< 20 kHz) is counted. Above it lies the half-band filters' transition
                // band, where the 5th harmonic (24.9 kHz, folding to 23.1 kHz) is legitimately only partly
                // attenuated whatever the oversampling factor; including it would put a floor under the metric.
                const size_t bandLimitBin = (size_t) (20000.0 / kFs * N);
                double fundamental = p[(size_t) fundamentalBin];
                double nonHarmonic = 0.0;
                for (size_t k = 1; k <= bandLimitBin; ++k)
                {
                    const bool oddHarmonic = (k % (size_t) fundamentalBin) == 0 && ((k / (size_t) fundamentalBin) % 2) == 1;
                    if (! oddHarmonic)
                        nonHarmonic += p[k];
                }
                aliasDb[idx] = toDb (nonHarmonic / fundamental);
            }
            logMessage (juce::String ("CEILING ") + (ceiling ? "on" : "off") + ", non-harmonic energy relative to the fundamental: 1x "
                        + juce::String (aliasDb[0], 1) + " dB, 2x " + juce::String (aliasDb[1], 1) + " dB, 4x "
                        + juce::String (aliasDb[2], 1) + " dB, 8x " + juce::String (aliasDb[3], 1) + " dB");
        };

        beginTest ("CEILING off: oversampling reduces non-harmonic (aliased) energy of a hard-clipped sine");
        {
            double aliasDb[4] = {};
            measure (false, aliasDb);
            expectLessThan (aliasDb[1], aliasDb[0] - 6.0, "2x is at least 6 dB cleaner than 1x");
            expectLessThan (aliasDb[2], aliasDb[1] - 6.0, "4x is at least 6 dB cleaner than 2x");
            expectLessThan (aliasDb[3], aliasDb[2] - 3.0, "8x is cleaner than 4x");
            expectLessThan (aliasDb[3], aliasDb[0] - 25.0, "8x is at least 25 dB cleaner than 1x");
        }

        beginTest ("CEILING on: every oversampled mode is still clearly cleaner than 1x");
        {
            double aliasDb[4] = {};
            measure (true, aliasDb);
            for (int idx = 1; idx < 4; ++idx)
                expectLessThan (aliasDb[idx], aliasDb[0] - 10.0, "oversampling index " + juce::String (idx) + " vs 1x");
        }
    }
};
static AliasingTests aliasingTests;

//==================================================================================================
struct MeteringTests : public juce::UnitTest
{
    MeteringTests() : juce::UnitTest ("Metering and waveform tap reflect the real signal", "Clipline-Engine") {}

    void runTest() override
    {
        beginTest ("input / output peaks, clip ratio and per-channel mapping");
        {
            auto s = base();
            s.clipDb = -6.0f; s.inputDb = 6.0f; s.outputDb = -3.0f; s.link = false;
            juce::AudioBuffer<float> in (2, 4800);
            for (int i = 0; i < 4800; ++i)
            {
                in.setSample (0, i, 0.9f * std::sin ((float) i * 0.1f));
                in.setSample (1, i, 0.1f * std::sin ((float) i * 0.1f));
            }
            ClipperEngine engine;
            engine.prepare (kFs, 512, 2);
            const auto out = render (engine, in, s, 100);

            auto& meters = engine.getMeters();
            expectEquals (meters.takeInputPeak (0), peakOf (in, 0));
            expectEquals (meters.takeInputPeak (1), peakOf (in, 1));
            expectEquals (meters.takeOutputPeak (0), peakOf (out, 0));
            expectEquals (meters.takeOutputPeak (1), peakOf (out, 1));
            const float expectedRatio = peakOf (in, 0) * ClipperEngine::dbToGain (6.0f) / tLin (-6.0f);
            expectWithinAbsoluteError (meters.takeClipRatio(), expectedRatio, 1.0e-4f);
            expectEquals (meters.takeClipRatio(), 0.0f, "cleared by the read");
        }

        beginTest ("clip ratio is below 1 when nothing is being clipped");
        {
            auto s = base();
            s.clipDb = 0.0f;
            ClipperEngine engine;
            engine.prepare (kFs, 512, 1);
            render (engine, makeSine (1, 4800, kFs, 440.0, 0.25f), s, 256);
            expectWithinAbsoluteError (engine.getMeters().takeClipRatio(), 0.25f, 2.0e-3f);
        }

        beginTest ("waveform tap carries the time-aligned clipper input and output");
        {
            auto s = base();
            s.clipDb = -6.0f; s.oversamplingIndex = 2;
            ClipperEngine engine;
            engine.prepare (kFs, 512, 1);
            const auto in = makeSine (1, 9600, kFs, 200.0, 1.0f);
            const auto out = render (engine, in, s, 256);
            (void) out;

            std::array<WaveformTap::Column, 4096> cols;
            std::uint64_t end = 0;
            const int n = engine.getWaveformTap().copyLatest (cols.data(), 4096, end);
            expectEquals (n, 3200);                         // 9600 samples / 3 per column

            float inMax = -10.0f, outMax = -10.0f, inMin = 10.0f, outMin = 10.0f;
            for (int i = 400; i < n; ++i)
            {
                inMax = std::max (inMax, cols[(size_t) i].inMax);   inMin = std::min (inMin, cols[(size_t) i].inMin);
                outMax = std::max (outMax, cols[(size_t) i].outMax); outMin = std::min (outMin, cols[(size_t) i].outMin);
            }
            expectWithinAbsoluteError (inMax, 1.0f, 2.0e-3f);
            expectWithinAbsoluteError (inMin, -1.0f, 2.0e-3f);
            expectLessThan (outMax, tLin (-6.0f) * 1.12f, "clipper output is limited");
            expectGreaterThan (outMax, tLin (-6.0f) * 0.98f, "...and reaches the threshold");
            expectLessThan (outMin, 0.0f);
        }
    }
};
static MeteringTests meteringTests;

//==================================================================================================
struct SoakTests : public juce::UnitTest
{
    SoakTests() : juce::UnitTest ("Long-run stability and CPU cost", "Clipline-Engine") {}

    void runTest() override
    {
        beginTest ("CPU cost per oversampling mode (stereo, 48 kHz, 512-sample blocks)");
        {
            juce::String report;
            for (int idx = 0; idx < 4; ++idx)
            {
                auto s = base();
                s.oversamplingIndex = idx; s.mode = ClipMode::Soft; s.link = true; s.inputDb = 6.0f; s.mixPercent = 80.0f;
                ClipperEngine engine;
                engine.prepare (kFs, 512, 2);
                auto buffer = makeNoise (2, 512, 0.5f, 3);
                const auto source = buffer;

                const int blocks = (int) (kFs * 20.0 / 512.0);          // 20 s of audio
                const auto t0 = std::chrono::steady_clock::now();
                for (int b = 0; b < blocks; ++b)
                {
                    for (int ch = 0; ch < 2; ++ch) buffer.copyFrom (ch, 0, source, ch, 0, 512);
                    engine.process (buffer, s);
                }
                const double seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
                const double loadPercent = 100.0 * seconds / 20.0;
                report << kOsNames[idx] << " " << juce::String (loadPercent, 2) << "% of one core   ";
                expectLessThan (loadPercent, 25.0, juce::String (kOsNames[idx]) + " must be far below real time");
            }
            logMessage ("CPU: " + report);
        }

        beginTest ("10 minutes of audio with random automation of every parameter and random host block sizes");
        {
            ClipperEngine engine;
            engine.prepare (kFs, 1024, 2);
            juce::Random rng (2024);

            EngineSettings s = base();
            const int totalSamples = (int) (kFs * 600.0);
            int done = 0, nextChange = 0;
            bool finite = true;
            float worst = 0.0f;
            juce::AudioBuffer<float> buffer (2, 2048);

            while (done < totalSamples && finite)
            {
                const int n = 1 + rng.nextInt (2048);

                if (done >= nextChange)
                {
                    s.inputDb  = rng.nextFloat() * 48.0f - 24.0f;
                    s.clipDb   = rng.nextFloat() * -24.0f;
                    s.outputDb = rng.nextFloat() * 48.0f - 24.0f;
                    s.mixPercent = rng.nextFloat() * 100.0f;
                    s.mode = kModes[(size_t) rng.nextInt (4)];
                    s.oversamplingIndex = rng.nextInt (4);
                    s.link = rng.nextBool();
                    nextChange = done + (int) (kFs * (0.002 + 0.4 * rng.nextDouble()));
                }

                // Mixed material: noise bursts, tones, silence and occasional huge (but finite) samples.
                const int kind = rng.nextInt (4);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < n; ++i)
                    {
                        float v = 0.0f;
                        if (kind == 0) v = (rng.nextFloat() * 2.0f - 1.0f) * 0.8f;
                        else if (kind == 1) v = std::sin ((float) (done + i) * 0.031f) * 1.5f;
                        else if (kind == 3) v = (rng.nextInt (500) == 0) ? (rng.nextFloat() * 2.0f - 1.0f) * 200.0f : 0.0f;
                        buffer.setSample (ch, i, v);
                    }

                juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(), 2, 0, n);
                engine.process (view, s);

                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < n; ++i)
                    {
                        const float v = view.getSample (ch, i);
                        finite = finite && isFiniteBits (v);
                        worst = std::max (worst, std::abs (v));
                    }
                done += n;
            }

            expect (finite, "all output samples finite for 10 minutes of audio");
            expectLessThan (worst, 1.0e5f, "output bounded (max |y| = " + juce::String (worst) + ")");
            expect (engine.getMeters().takeOutputPeak (0) >= 0.0f);
        }
    }
};
static SoakTests soakTests;
