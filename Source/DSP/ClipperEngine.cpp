#include "ClipperEngine.h"

#include "../ParameterIds.h"

#include <algorithm>
#include <cmath>

namespace clipline::dsp
{
    //==============================================================================================
    // Shaper kernels
    //==============================================================================================
    namespace
    {
        inline float clampTo (float y, float t) noexcept
        {
            return y > t ? t : (y < -t ? -t : y);
        }

        /** HARD, channels independent: y = clamp(x, -T, +T). Bit-exact pass-through below threshold. */
        float hardUnlinked (float* const* d, int nCh, int n, const float* thr, const float* inv) noexcept
        {
            float maxRatio = 0.0f;
            for (int ch = 0; ch < nCh; ++ch)
            {
                float* x = d[ch];
                for (int i = 0; i < n; ++i)
                {
                    const float t = thr[i];
                    const float v = x[i];
                    maxRatio = std::max (maxRatio, std::abs (v) * inv[i]);
                    x[i] = clampTo (v, t);
                }
            }
            return maxRatio;
        }

        /** Smooth shapers, channels independent: y = T * f (x / T). */
        template <typename Shape>
        float shapedUnlinked (float* const* d, int nCh, int n, const float* thr, const float* inv, Shape shape) noexcept
        {
            float maxRatio = 0.0f;
            for (int ch = 0; ch < nCh; ++ch)
            {
                float* x = d[ch];
                for (int i = 0; i < n; ++i)
                {
                    const float t = thr[i];
                    const float u = x[i] * inv[i];
                    maxRatio = std::max (maxRatio, std::abs (u));
                    x[i] = clampTo (shape (u) * t, t);
                }
            }
            return maxRatio;
        }

        /** HARD, linked: both channels are scaled by min (1, T / max(|L|,|R|)). */
        float hardLinked (float* const* d, int n, const float* thr, const float* inv) noexcept
        {
            float* l = d[0];
            float* r = d[1];
            float maxRatio = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float t = thr[i];
                const float u = std::max (std::abs (l[i]), std::abs (r[i])) * inv[i];
                maxRatio = std::max (maxRatio, u);
                const float g = u > 1.0f ? 1.0f / u : 1.0f;
                l[i] = clampTo (l[i] * g, t);
                r[i] = clampTo (r[i] * g, t);
            }
            return maxRatio;
        }

        /** Smooth shapers, linked: common gain g = f(u) / u from the louder channel. */
        template <typename Shape>
        float shapedLinked (float* const* d, int n, const float* thr, const float* inv, Shape shape) noexcept
        {
            float* l = d[0];
            float* r = d[1];
            float maxRatio = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float t = thr[i];
                const float u = std::max (std::abs (l[i]), std::abs (r[i])) * inv[i];
                maxRatio = std::max (maxRatio, u);
                const float g = u > 1.0e-6f ? shape (u) / u : 1.0f;     // every f has f'(0) = 1
                l[i] = clampTo (l[i] * g, t);
                r[i] = clampTo (r[i] * g, t);
            }
            return maxRatio;
        }
    }

    float applyClipper (float* const* data, int numChannels, int numSamples,
                        const float* thr, const float* invThr, ShaperConfig config) noexcept
    {
        if (numChannels <= 0 || numSamples <= 0)
            return 0.0f;

        if (config.link && numChannels >= 2)
        {
            switch (config.mode)
            {
                case ClipMode::Hard:  return hardLinked (data, numSamples, thr, invThr);
                case ClipMode::Soft:  return shapedLinked (data, numSamples, thr, invThr, [] (float u) { return softClip (u); });
                case ClipMode::Cubic: return shapedLinked (data, numSamples, thr, invThr, [] (float u) { return cubicClip (u); });
                case ClipMode::Fold:  return shapedLinked (data, numSamples, thr, invThr, [] (float u) { return foldClip (u); });
            }
        }

        switch (config.mode)
        {
            case ClipMode::Hard:  return hardUnlinked (data, numChannels, numSamples, thr, invThr);
            case ClipMode::Soft:  return shapedUnlinked (data, numChannels, numSamples, thr, invThr, [] (float u) { return softClip (u); });
            case ClipMode::Cubic: return shapedUnlinked (data, numChannels, numSamples, thr, invThr, [] (float u) { return cubicClip (u); });
            case ClipMode::Fold:  return shapedUnlinked (data, numChannels, numSamples, thr, invThr, [] (float u) { return foldClip (u); });
        }
        return 0.0f;
    }

    //==============================================================================================
    // Engine
    //==============================================================================================
    ClipperEngine::ClipperEngine() = default;

    void ClipperEngine::prepare (double newSampleRate, int maxBlockSize, int newNumChannels)
    {
        prepared = false;

        sampleRate  = newSampleRate > 0.0 ? newSampleRate : 48000.0;
        maxBlock    = std::max (1, maxBlockSize);
        numChannels = juce::jlimit (1, kMaxChannels, newNumChannels);
        fadeSamples = std::max (1, (int) std::lround (kFadeSeconds * sampleRate));

        oversampler.prepare (numChannels, maxBlock);

        historyCapacity = 2 * oversampler.getMaxLatencySamples() + 64;
        ring.prepare (numChannels, historyCapacity, maxBlock);

        const auto blockSize   = (size_t) maxBlock;
        const auto osBlockSize = blockSize * (size_t) kMaxFactor;

        for (int ch = 0; ch < kMaxChannels; ++ch)
        {
            const auto c = (size_t) ch;
            aBuf[c].allocate (blockSize, true);
            dryNew[c].allocate (blockSize, true);
            wetNew[c].allocate (blockSize, true);
            dryOld[c].allocate (blockSize, true);
            wetOld[c].allocate (blockSize, true);
            history[c].allocate ((size_t) historyCapacity, true);
            altOs[c].allocate (osBlockSize, true);
        }

        for (auto* block : { &gIn, &gOut, &mixA, &thrBase, &invBase, &modeFade, &osFade,
                             &tapInMin, &tapInMax, &tapOutMin, &tapOutMax })
            block->allocate (blockSize, true);

        for (auto* block : { &thrOs, &invOs, &modeFadeOs })
            block->allocate (osBlockSize, true);

        for (auto* s : { &inGain, &outGain, &threshold })
            s->reset (sampleRate, kRampSeconds);
        mix.reset (sampleRate, kRampSeconds);

        tap.prepare (sampleRate);
        reset();
        prepared = true;
    }

    void ClipperEngine::reset()
    {
        initialised = false;
        osFadeRemaining = 0;
        modeFadeRemaining = 0;
        ring.reset();
        meters.reset();

        for (int index = 0; index < OversamplingProcessor::kNumModes; ++index)
            oversampler.reset (index);
    }

    void ClipperEngine::process (juce::AudioBuffer<float>& buffer, const EngineSettings& settings) noexcept
    {
        if (! prepared)
            return;

        juce::ScopedNoDenormals noDenormals;

        const int numCh = std::min (buffer.getNumChannels(), numChannels);
        const int total = buffer.getNumSamples();
        if (numCh <= 0 || total <= 0)
            return;

        float* channels[kMaxChannels] = {};
        for (int ch = 0; ch < numCh; ++ch)
            channels[ch] = buffer.getWritePointer (ch);

        for (int pos = 0; pos < total; pos += maxBlock)
        {
            const int n = std::min (maxBlock, total - pos);
            float* chunk[kMaxChannels] = {};
            for (int ch = 0; ch < numCh; ++ch)
                chunk[ch] = channels[ch] + pos;

            processChunk (chunk, numCh, n, settings);
        }
    }

    void ClipperEngine::warmUpPath (int index, int numCh, const float* const* hist, int count, float thresholdLinear) noexcept
    {
        const int factor = OversamplingProcessor::factorForIndex (index);

        for (int offset = 0; offset < count; offset += maxBlock)
        {
            const int m = std::min (maxBlock, count - offset);
            const int mOs = m * factor;

            const float* in[kMaxChannels] = {};
            for (int ch = 0; ch < numCh; ++ch)
                in[ch] = hist[ch] + offset;

            auto os = oversampler.processUp (index, juce::dsp::AudioBlock<const float> (in, (size_t) numCh, (size_t) m));

            float* osPtr[kMaxChannels] = {};
            for (int ch = 0; ch < numCh; ++ch)
                osPtr[ch] = os.getChannelPointer ((size_t) ch);

            std::fill (thrOs.get(), thrOs.get() + mOs, thresholdLinear);
            std::fill (invOs.get(), invOs.get() + mOs, 1.0f / thresholdLinear);
            applyClipper (osPtr, numCh, mOs, thrOs, invOs, config);

            float* out[kMaxChannels] = {};
            for (int ch = 0; ch < numCh; ++ch)
                out[ch] = wetNew[(size_t) ch].get();

            juce::dsp::AudioBlock<float> outBlock (out, (size_t) numCh, (size_t) m);
            oversampler.processDown (index, outBlock);       // output is discarded: this only primes the filters
        }
    }

    void ClipperEngine::beginOversamplingSwitch (int newIndex, float thresholdLinear) noexcept
    {
        previousOs = activeOs;
        activeOs = newIndex;
        oversampler.reset (newIndex);

        // The FIR chains have finite memory: feeding them the input that preceded this block makes them
        // produce exactly what they would have produced had they been running all along.
        const int factor = OversamplingProcessor::factorForIndex (newIndex);
        if (factor > 1)
        {
            const int count = std::min (historyCapacity, 2 * oversampler.getLatencySamples (newIndex) + 32);

            const float* hist[kMaxChannels] = {};
            for (int ch = 0; ch < numChannels; ++ch)
            {
                ring.readHistory (ch, history[(size_t) ch].get(), count);
                hist[ch] = history[(size_t) ch].get();
            }

            warmUpPath (newIndex, numChannels, hist, count, thresholdLinear);
        }

        osFadeRemaining = fadeSamples;
    }

    void ClipperEngine::runWetPath (int index, int numCh, int n, bool crossfadeShaper, float* const* wetOut) noexcept
    {
        const int factor = OversamplingProcessor::factorForIndex (index);
        const int nOs = n * factor;

        const float* in[kMaxChannels] = {};
        for (int ch = 0; ch < numCh; ++ch)
            in[ch] = aBuf[(size_t) ch].get();

        auto os = oversampler.processUp (index, juce::dsp::AudioBlock<const float> (in, (size_t) numCh, (size_t) n));

        float* osPtr[kMaxChannels] = {};
        for (int ch = 0; ch < numCh; ++ch)
            osPtr[ch] = os.getChannelPointer ((size_t) ch);

        const float* thr = thrBase.get();
        const float* inv = invBase.get();
        const float* fade = modeFade.get();

        if (factor > 1)
        {
            // Hold each base-rate control value for `factor` oversampled samples. The values change slowly
            // (20 ms ramps), so the staircase error is far below anything audible.
            for (int i = 0; i < n; ++i)
            {
                const int base = i * factor;
                for (int k = 0; k < factor; ++k)
                {
                    thrOs[base + k] = thrBase[i];
                    invOs[base + k] = invBase[i];
                    modeFadeOs[base + k] = modeFade[i];
                }
            }
            thr = thrOs.get();
            inv = invOs.get();
            fade = modeFadeOs.get();
        }

        float ratio;
        if (crossfadeShaper)
        {
            // Shaper configuration changed recently: blend the previous and the new transfer function.
            float* alt[kMaxChannels] = {};
            for (int ch = 0; ch < numCh; ++ch)
            {
                alt[ch] = altOs[(size_t) ch].get();
                std::copy (osPtr[ch], osPtr[ch] + nOs, alt[ch]);
            }

            applyClipper (alt, numCh, nOs, thr, inv, previousConfig);
            ratio = applyClipper (osPtr, numCh, nOs, thr, inv, config);

            for (int ch = 0; ch < numCh; ++ch)
                for (int j = 0; j < nOs; ++j)
                    osPtr[ch][j] = alt[ch][j] + (osPtr[ch][j] - alt[ch][j]) * fade[j];
        }
        else
        {
            ratio = applyClipper (osPtr, numCh, nOs, thr, inv, config);
        }

        meters.pushClipRatio (ratio);

        juce::dsp::AudioBlock<float> outBlock (wetOut, (size_t) numCh, (size_t) n);
        oversampler.processDown (index, outBlock);

        if (factor > 1 && ceilingSafety)
        {
            // Ceiling safety stage (CEILING on). The down-sampling filter is a linear-phase band-limiter, so the
            // clipped signal rings (Gibbs) and its filtered peak can exceed the threshold: by ~1.5 dB on clipped
            // tones above 10 kHz and by up to ~4 dB on dense, noise-like material such as clipped hi-hats. A
            // second, base-rate clamp at the same threshold makes CLIP a real ceiling in every mode. The price:
            // flattening those overshoot peaks aliases again, so on heavily clipped high-frequency material
            // CEILING off is cleaner (docs/ALGORITHMS.md has the measurements).
            for (int ch = 0; ch < numCh; ++ch)
            {
                float* w = wetOut[ch];
                for (int i = 0; i < n; ++i)
                    w[i] = juce::jlimit (-thrBase[i], thrBase[i], w[i]);
            }
        }
    }

    void ClipperEngine::processChunk (float* const* io, int numCh, int n, const EngineSettings& s) noexcept
    {
        // ---- validate / clamp settings ---------------------------------------------------------------
        auto sane = [] (float v, float lo, float hi, float fallback)
        {
            return isFiniteBits (v) ? juce::jlimit (lo, hi, v) : fallback;
        };

        ceilingSafety = s.ceiling;

        const float inLin   = dbToGain (sane (s.inputDb,  params::kGainMinDb, params::kGainMaxDb, 0.0f));
        const float outLin  = dbToGain (sane (s.outputDb, params::kGainMinDb, params::kGainMaxDb, 0.0f));
        const float thrLin  = dbToGain (sane (s.clipDb,   params::kClipMinDb, params::kClipMaxDb, params::kClipDefaultDb));
        const float mixLin  = sane (s.mixPercent, params::kMixMinPercent, params::kMixMaxPercent, 100.0f) * 0.01f;
        const int   osIndex = OversamplingProcessor::clampIndex (s.oversamplingIndex);
        const ShaperConfig wanted { s.mode, s.link };

        if (! initialised)
        {
            inGain.setCurrentAndTargetValue (inLin);
            outGain.setCurrentAndTargetValue (outLin);
            threshold.setCurrentAndTargetValue (thrLin);
            mix.setCurrentAndTargetValue (mixLin);
            activeOs = previousOs = osIndex;
            config = previousConfig = wanted;
            oversampler.reset (activeOs);
            ring.reset();
            osFadeRemaining = modeFadeRemaining = 0;
            initialised = true;
        }
        else
        {
            inGain.setTargetValue (inLin);
            outGain.setTargetValue (outLin);
            threshold.setTargetValue (thrLin);
            mix.setTargetValue (mixLin);

            // Transitions are applied one at a time; a change that arrives during a fade is picked up
            // as soon as the fade has finished.
            if (osIndex != activeOs && osFadeRemaining == 0)
                beginOversamplingSwitch (osIndex, threshold.getCurrentValue());

            if (wanted != config && modeFadeRemaining == 0)
            {
                previousConfig = config;
                config = wanted;
                modeFadeRemaining = fadeSamples;
            }
        }

        // ---- per-sample control signals --------------------------------------------------------------
        for (int i = 0; i < n; ++i)
        {
            gIn[i]  = inGain.getNextValue();
            gOut[i] = outGain.getNextValue();
            mixA[i] = mix.getNextValue();
            const float t = threshold.getNextValue();
            thrBase[i] = t;
            invBase[i] = 1.0f / t;
        }

        const bool modeFading = modeFadeRemaining > 0;
        if (modeFading)
        {
            for (int i = 0; i < n; ++i)
            {
                if (modeFadeRemaining > 0)
                {
                    modeFade[i] = 1.0f - (float) modeFadeRemaining / (float) fadeSamples;
                    --modeFadeRemaining;
                }
                else
                {
                    modeFade[i] = 1.0f;
                }
            }
        }

        const bool osFading = osFadeRemaining > 0;
        if (osFading)
        {
            for (int i = 0; i < n; ++i)
            {
                if (osFadeRemaining > 0)
                {
                    osFade[i] = 1.0f - (float) osFadeRemaining / (float) fadeSamples;
                    --osFadeRemaining;
                }
                else
                {
                    osFade[i] = 1.0f;
                }
            }
        }

        // ---- input stage: sanitise, meter, INPUT gain -----------------------------------------------------
        for (int ch = 0; ch < numCh; ++ch)
        {
            const float* x = io[ch];
            float* a = aBuf[(size_t) ch].get();
            float peak = 0.0f;

            for (int i = 0; i < n; ++i)
            {
                const float raw = sanitize (x[i]);
                peak = std::max (peak, std::abs (raw));
                a[i] = sanitize (raw * gIn[i]);
            }

            meters.pushInputPeak (ch, peak);
        }

        const float* aPtr[kMaxChannels] = {};
        for (int ch = 0; ch < numCh; ++ch)
            aPtr[ch] = aBuf[(size_t) ch].get();

        ring.push (aPtr, numCh, n);

        // ---- wet paths --------------------------------------------------------------------------------------
        float* wetNewPtr[kMaxChannels] = {};
        float* wetOldPtr[kMaxChannels] = {};
        for (int ch = 0; ch < numCh; ++ch)
        {
            wetNewPtr[ch] = wetNew[(size_t) ch].get();
            wetOldPtr[ch] = wetOld[(size_t) ch].get();
        }

        runWetPath (activeOs, numCh, n, modeFading, wetNewPtr);
        if (osFading)
            runWetPath (previousOs, numCh, n, modeFading, wetOldPtr);

        // ---- time-aligned dry paths ------------------------------------------------------------------
        const int latencyNew = oversampler.getLatencySamples (activeOs);
        const int latencyOld = oversampler.getLatencySamples (previousOs);
        for (int ch = 0; ch < numCh; ++ch)
        {
            ring.read (ch, dryNew[(size_t) ch].get(), n, latencyNew);
            if (osFading)
                ring.read (ch, dryOld[(size_t) ch].get(), n, latencyOld);
        }

        // ---- mix, OUTPUT gain, output sanitising, metering ---------------------------------------------
        for (int ch = 0; ch < numCh; ++ch)
        {
            const float* dn = dryNew[(size_t) ch].get();
            const float* wn = wetNew[(size_t) ch].get();
            const float* dO = dryOld[(size_t) ch].get();
            const float* wO = wetOld[(size_t) ch].get();
            float* out = io[ch];
            float peak = 0.0f;

            for (int i = 0; i < n; ++i)
            {
                const float m = mixA[i];
                float y = (1.0f - m) * dn[i] + m * wn[i];

                if (osFading)
                {
                    const float yOld = (1.0f - m) * dO[i] + m * wO[i];
                    y = yOld + (y - yOld) * osFade[i];
                }

                y = sanitize (y * gOut[i]);
                out[i] = y;
                peak = std::max (peak, std::abs (y));
            }

            meters.pushOutputPeak (ch, peak);
        }

        // ---- waveform display tap: aligned clipper input vs clipper output ------------------------------------
        for (int i = 0; i < n; ++i)
        {
            float inLo = dryNew[0][i], inHi = inLo, outLo = wetNew[0][i], outHi = outLo;
            if (numCh > 1)
            {
                inLo = std::min (inLo, dryNew[1][i]);   inHi = std::max (inHi, dryNew[1][i]);
                outLo = std::min (outLo, wetNew[1][i]); outHi = std::max (outHi, wetNew[1][i]);
            }
            tapInMin[i] = inLo;  tapInMax[i] = inHi;
            tapOutMin[i] = outLo; tapOutMax[i] = outHi;
        }
        tap.push (tapInMin, tapInMax, tapOutMin, tapOutMax, n);
    }
}
