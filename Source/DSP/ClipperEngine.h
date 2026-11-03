#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <array>

#include "ClippingFunctions.h"
#include "DelayLine.h"
#include "Metering.h"
#include "NumericSafety.h"
#include "OversamplingProcessor.h"
#include "WaveformTap.h"

namespace clipline::dsp
{
    /** Which transfer function is applied and whether the stereo channels share one gain. */
    struct ShaperConfig
    {
        ClipMode mode = ClipMode::Hard;
        bool link = true;

        bool operator== (const ShaperConfig& o) const noexcept { return mode == o.mode && link == o.link; }
        bool operator!= (const ShaperConfig& o) const noexcept { return ! (*this == o); }
    };

    /** Applies the clipper in place to `numChannels` (1 or 2) channels of already (over)sampled data.

        thr[i] is the linear clip threshold T and invThr[i] = 1/T for sample i.

        UNLINKED (or mono):  y = T * f(x / T) per channel.
        LINKED (stereo):     u = max(|L|, |R|) / T ;  g = f(u) / u ;  L' = g L ; R' = g R
                             i.e. one common gain computed from the louder channel, so the instantaneous
                             L/R ratio - and with it the stereo image - is preserved.

        In both cases the result is finally clamped to [-T, +T]: |y| <= T always holds exactly.

        @returns max(|x|) / T over the block (> 1 means the clipper is actively limiting). */
    float applyClipper (float* const* data, int numChannels, int numSamples,
                        const float* thr, const float* invThr, ShaperConfig config) noexcept;

    //==============================================================================================
    /** Everything the engine needs per block, in the units shown to the user. */
    struct EngineSettings
    {
        float inputDb = 0.0f;
        float clipDb = -3.0f;
        float outputDb = 0.0f;
        float mixPercent = 100.0f;
        ClipMode mode = ClipMode::Hard;
        int oversamplingIndex = 0;      // 0..3 -> 1x, 2x, 4x, 8x
        bool link = true;
        bool ceiling = true;            // base-rate safety clamp after down-sampling (oversampled modes)
    };

    /** The complete, UI-independent signal path.

        Processing order (per sample):

            in --> sanitize --> x INPUT gain --> a  ------------------------------+--> delay(L) --> dry
                                                 |                                |
                                                 +--> upsample --> CLIP --> downsample --> wet
                                                                                              |
            out <-- sanitize <-- x OUTPUT gain <-- mix:  (1 - MIX) * dry + MIX * wet <--------+

        L is the latency of the active oversampling mode; the dry path is delayed by exactly L samples
        so the two paths are time aligned for every MIX setting. Real-time safe after prepare(). */
    class ClipperEngine
    {
    public:
        static constexpr int kMaxChannels = 2;
        static constexpr double kRampSeconds = 0.020;    // parameter smoothing
        static constexpr double kFadeSeconds = 0.005;    // mode / oversampling transitions

        ClipperEngine();

        /** Allocates. Not real-time safe. numChannels = 1 or 2. */
        void prepare (double sampleRate, int maxBlockSize, int numChannels);

        /** Clears all audio state (filters, delay, smoothing snaps to the next settings). */
        void reset();

        /** Processes `buffer` in place. Blocks larger than the prepared size are handled in chunks. */
        void process (juce::AudioBuffer<float>& buffer, const EngineSettings& settings) noexcept;

        /** Latency of the oversampling mode that is currently active, in samples. */
        int getLatencySamples() const noexcept { return oversampler.getLatencySamples (activeOs); }
        int getLatencySamplesFor (int oversamplingIndex) const noexcept { return oversampler.getLatencySamples (oversamplingIndex); }
        int getActiveOversamplingIndex() const noexcept { return activeOs; }

        MeterState& getMeters() noexcept       { return meters; }
        WaveformTap& getWaveformTap() noexcept { return tap; }

        static float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }

    private:
        static constexpr int kMaxFactor = OversamplingProcessor::kMaxFactor;

        void processChunk (float* const* io, int numCh, int n, const EngineSettings& s) noexcept;
        void beginOversamplingSwitch (int newIndex, float thresholdLinear) noexcept;
        void runWetPath (int index, int numCh, int n, bool crossfadeShaper, float* const* wetOut) noexcept;
        void warmUpPath (int index, int numCh, const float* const* history, int count, float thresholdLinear) noexcept;

        double sampleRate = 48000.0;
        int maxBlock = 0;
        int numChannels = 0;
        int fadeSamples = 240;
        bool prepared = false;
        bool ceilingSafety = true;
        bool initialised = false;

        OversamplingProcessor oversampler;
        DelayLine ring;               // holds `a` (signal after INPUT gain) for dry alignment and warm-up
        int activeOs = 0;
        int previousOs = 0;
        ShaperConfig config, previousConfig;
        int osFadeRemaining = 0;
        int modeFadeRemaining = 0;

        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> inGain, outGain, threshold;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mix;

        // Scratch (sized in prepare()).
        using Plane = std::array<juce::HeapBlock<float>, kMaxChannels>;
        Plane aBuf, dryNew, wetNew, dryOld, wetOld, history;
        juce::HeapBlock<float> gIn, gOut, mixA, thrBase, invBase, modeFade, osFade;
        juce::HeapBlock<float> thrOs, invOs, modeFadeOs;
        Plane altOs;
        juce::HeapBlock<float> tapInMin, tapInMax, tapOutMin, tapOutMax;
        int historyCapacity = 0;

        MeterState meters;
        WaveformTap tap;
    };
}
