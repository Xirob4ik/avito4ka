#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <memory>

namespace clipline::dsp
{
    /** Owns one juce::dsp::Oversampling chain per mode (1x, 2x, 4x, 8x) and exposes them by index.

        Why a custom chain instead of juce::dsp::Oversampling's convenience constructor:
          - Every stage is a linear-phase equiripple half-band FIR, so the wet path has a symmetric
            impulse response and a pure delay (no phase distortion that would comb against the dry path).
          - The stage designs in stageDesigns() are chosen so that the *total* latency of every mode
            is an exact integer number of samples (JUCE's default designs give fractional latencies for
            4x and 8x). The dry path can then be delayed by exactly the same amount, and the latency
            reported to the host is exact. See docs/ALGORITHMS.md and the "latency is integer" test.
          - FIR filters have finite memory, so a chain can be "warmed up" from recent input history and
            then produces exactly the output it would have produced had it been running all along.
            ClipperEngine uses this to switch oversampling modes without clicks.

        Index <-> factor: 0 -> 1x, 1 -> 2x, 2 -> 4x, 3 -> 8x. */
    class OversamplingProcessor
    {
    public:
        static constexpr int kNumModes  = 4;
        static constexpr int kMaxFactor = 8;
        static constexpr int kNumStages = 3;

        /** Half-band FIR design parameters for one 2x stage (see juce::dsp::FilterDesign). */
        struct StageDesign
        {
            float transitionUp;
            float stopbandUpDb;
            float transitionDown;
            float stopbandDownDb;
        };

        /** The fixed design table. Stage n is used by every mode with factor >= 2^(n+1). */
        static const std::array<StageDesign, kNumStages>& stageDesigns() noexcept;

        /** Total latency of a chain of the first `numStages` stages of `designs`, in 1/8 sample units
            (base-rate samples). The value is a multiple of 8 exactly when the latency is an integer.
            Allocates (designs filters): for tests / offline design search only. */
        static int designLatencyEighths (const std::array<StageDesign, kNumStages>& designs, int numStages);

        static int clampIndex (int index) noexcept { return index < 0 ? 0 : (index >= kNumModes ? kNumModes - 1 : index); }
        static int factorForIndex (int index) noexcept { return 1 << clampIndex (index); }

        //==========================================================================================
        /** Allocates every chain. Not real-time safe. */
        void prepare (int numChannels, int maxBlockSize);

        /** Clears the filter state of one chain. */
        void reset (int index) noexcept;

        /** Exact (integer) latency of a mode in base-rate samples. Valid after prepare(). */
        int getLatencySamples (int index) const noexcept { return latencies[(size_t) clampIndex (index)]; }
        int getMaxLatencySamples() const noexcept        { return latencies[(size_t) (kNumModes - 1)]; }

        /** Upsamples `input`. Returns a block of input.numSamples * factor samples that is owned by the chain;
            the caller may modify it in place and must then call processDown() for the same mode. */
        juce::dsp::AudioBlock<float> processUp (int index, const juce::dsp::AudioBlock<const float>& input) noexcept;

        /** Low-pass filters and decimates the (modified) block returned by processUp() into `output`. */
        void processDown (int index, juce::dsp::AudioBlock<float>& output) noexcept;

    private:
        std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, kNumModes> chains;
        std::array<int, kNumModes> latencies {};
    };
}
