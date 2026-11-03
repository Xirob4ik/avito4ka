#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "DSP/ClipperEngine.h"

namespace cltest
{
    using namespace clipline::dsp;

    constexpr double kPi = 3.14159265358979323846;

    inline juce::AudioBuffer<float> makeSine (int channels, int numSamples, double fs, double freq, float amp,
                                              double phase = 0.0)
    {
        juce::AudioBuffer<float> b (channels, numSamples);
        for (int ch = 0; ch < channels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                b.setSample (ch, i, amp * (float) std::sin (2.0 * kPi * freq * (double) i / fs + phase));
        return b;
    }

    inline juce::AudioBuffer<float> makeNoise (int channels, int numSamples, float amp, int seed = 1234)
    {
        juce::Random rng (seed);
        juce::AudioBuffer<float> b (channels, numSamples);
        for (int ch = 0; ch < channels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                b.setSample (ch, i, amp * (rng.nextFloat() * 2.0f - 1.0f));
        return b;
    }

    /** Synthetic programme material for tests and screenshots: "drums" (kick, snare, hats), "bass" (55 Hz + octave)
        or "mix" (tone + kick + snare). Peaks reach about 1.0-1.2, i.e. loud enough to be clipped by the presets. */
    inline void renderMaterial (juce::AudioBuffer<float>& b, const juce::String& kind, double fs)
    {
        juce::Random rng (7);
        const int n = b.getNumSamples();
        for (int i = 0; i < n; ++i)
        {
            const double t = (double) i / fs;
            float l = 0.0f, r = 0.0f;

            if (kind == "bass")
            {
                const double f = 55.0;
                const float v = (float) (std::sin (2.0 * kPi * f * t) * 0.9 + std::sin (2.0 * kPi * 2.0 * f * t) * 0.35);
                l = r = v * 0.8f;
            }
            else if (kind == "mix")
            {
                const float tone = (float) (std::sin (2.0 * kPi * 220.0 * t) * 0.3 + std::sin (2.0 * kPi * 880.0 * t + 1.0) * 0.12);
                const double tk = std::fmod (t, 0.5);
                const float kick = (float) (std::sin (2.0 * kPi * (50.0 + 90.0 * std::exp (-tk * 40.0)) * tk) * std::exp (-tk * 7.0) * 0.95);
                const double ts = std::fmod (t + 0.25, 0.5);
                const float snare = (rng.nextFloat() * 2.0f - 1.0f) * (float) std::exp (-ts * 22.0) * 0.5f;
                l = tone + kick + snare;
                r = tone * 0.8f + kick + snare * 0.7f;
            }
            else    // drums: kick every 0.5 s, snare noise on the off beat, hats
            {
                const double tk = std::fmod (t, 0.5);
                const float kick = (float) (std::sin (2.0 * kPi * (45.0 + 110.0 * std::exp (-tk * 45.0)) * tk) * std::exp (-tk * 9.0) * 1.1);
                const double ts = std::fmod (t + 0.25, 0.5);
                const float snare = (float) ((rng.nextFloat() * 2.0f - 1.0f) * std::exp (-ts * 18.0) * 0.55
                                             + std::sin (2.0 * kPi * 190.0 * ts) * std::exp (-ts * 20.0) * 0.5);
                const double th = std::fmod (t, 0.125);
                const float hat = (rng.nextFloat() * 2.0f - 1.0f) * (float) std::exp (-th * 90.0) * 0.18f;
                l = kick + snare + hat;
                r = kick + snare * 0.8f + hat * 1.2f;
            }

            b.setSample (0, i, l);
            if (b.getNumChannels() > 1)
                b.setSample (1, i, r);
        }
    }


    inline float peakOf (const juce::AudioBuffer<float>& b, int ch, int start = 0, int count = -1)
    {
        if (count < 0) count = b.getNumSamples() - start;
        return b.getMagnitude (ch, start, count);
    }

    inline bool allFinite (const juce::AudioBuffer<float>& b)
    {
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = 0; i < b.getNumSamples(); ++i)
                if (! isFiniteBits (b.getSample (ch, i)))
                    return false;
        return true;
    }

    /** Largest sample-to-sample difference on one channel over [start, end). */
    inline float maxStep (const juce::AudioBuffer<float>& b, int ch, int start, int end)
    {
        const float* x = b.getReadPointer (ch);
        float m = 0.0f;
        for (int i = std::max (1, start); i < end; ++i)
            m = std::max (m, std::abs (x[i] - x[i - 1]));
        return m;
    }

    inline double meanOf (const juce::AudioBuffer<float>& b, int ch, int start, int count)
    {
        double s = 0.0;
        for (int i = 0; i < count; ++i)
            s += (double) b.getSample (ch, start + i);
        return s / (double) count;
    }

    inline double rmsDiff (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b, int ch, int start, int count)
    {
        double s = 0.0;
        for (int i = 0; i < count; ++i)
        {
            const double d = (double) a.getSample (ch, start + i) - (double) b.getSample (ch, start + i);
            s += d * d;
        }
        return std::sqrt (s / (double) count);
    }

    inline float maxAbsDiff (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b, int ch, int start, int count)
    {
        float m = 0.0f;
        for (int i = 0; i < count; ++i)
            m = std::max (m, std::abs (a.getSample (ch, start + i) - b.getSample (ch, start + i)));
        return m;
    }

    /** Runs `engine` over `input` in blocks of `blockSize` samples and returns the processed copy. */
    inline juce::AudioBuffer<float> render (ClipperEngine& engine, const juce::AudioBuffer<float>& input,
                                            const EngineSettings& settings, int blockSize)
    {
        juce::AudioBuffer<float> out (input);
        for (int pos = 0; pos < out.getNumSamples(); pos += blockSize)
        {
            const int n = std::min (blockSize, out.getNumSamples() - pos);
            juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), out.getNumChannels(), pos, n);
            engine.process (view, settings);
        }
        return out;
    }

    /** One-shot: fresh engine, constant settings. */
    inline juce::AudioBuffer<float> renderFresh (const juce::AudioBuffer<float>& input, const EngineSettings& settings,
                                                 double fs = 48000.0, int blockSize = 256, int maxBlock = 512)
    {
        ClipperEngine engine;
        engine.prepare (fs, maxBlock, input.getNumChannels());
        return render (engine, input, settings, blockSize);
    }

    /** Power spectrum (bins 0..N/2) of N samples, no window: use only with coherently sampled signals. */
    inline std::vector<double> powerSpectrum (const float* x, int N)
    {
        int order = 0;
        while ((1 << order) < N) ++order;
        jassert ((1 << order) == N);

        juce::dsp::FFT fft (order);
        std::vector<float> data ((size_t) N * 2, 0.0f);
        std::copy (x, x + N, data.begin());
        fft.performRealOnlyForwardTransform (data.data());

        std::vector<double> p ((size_t) N / 2 + 1);
        for (size_t k = 0; k < p.size(); ++k)
        {
            const double re = data[2 * k], im = data[2 * k + 1];
            p[k] = re * re + im * im;
        }
        return p;
    }

    inline double toDb (double ratio) { return 10.0 * std::log10 (std::max (ratio, 1.0e-30)); }
    inline double ampToDb (double a)  { return 20.0 * std::log10 (std::max (a, 1.0e-30)); }
}
