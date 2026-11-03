#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

namespace clipline::dsp
{
    /** Lock-free values published by the audio thread and consumed by the UI thread.

        Every value is a "maximum since the UI last asked" accumulator: the audio thread raises it
        with a compare-exchange loop and the UI reads-and-clears it with exchange(0). No locks, no
        allocation, and no peak can be lost between two UI frames. */
    class MeterState
    {
    public:
        static constexpr int kMaxChannels = 2;

        MeterState() noexcept { reset(); }

        void reset() noexcept
        {
            for (auto& v : inputPeak)  v.store (0.0f, std::memory_order_relaxed);
            for (auto& v : outputPeak) v.store (0.0f, std::memory_order_relaxed);
            clipOvershoot.store (0.0f, std::memory_order_relaxed);
        }

        // ---- audio thread ----------------------------------------------------------------------
        void pushInputPeak  (int ch, float linearPeak) noexcept { raise (inputPeak[(size_t) ch],  linearPeak); }
        void pushOutputPeak (int ch, float linearPeak) noexcept { raise (outputPeak[(size_t) ch], linearPeak); }

        /** `ratio` = |x| / threshold measured on the (oversampled) signal that enters the clipper.
            Values above 1 mean the clipper is actively limiting. */
        void pushClipRatio (float ratio) noexcept { raise (clipOvershoot, ratio); }

        // ---- UI thread (read and clear) --------------------------------------------------------
        float takeInputPeak  (int ch) noexcept { return inputPeak[(size_t) ch].exchange (0.0f, std::memory_order_relaxed); }
        float takeOutputPeak (int ch) noexcept { return outputPeak[(size_t) ch].exchange (0.0f, std::memory_order_relaxed); }
        float takeClipRatio() noexcept         { return clipOvershoot.exchange (0.0f, std::memory_order_relaxed); }

    private:
        static void raise (std::atomic<float>& target, float value) noexcept
        {
            float current = target.load (std::memory_order_relaxed);
            while (value > current
                   && ! target.compare_exchange_weak (current, value, std::memory_order_relaxed))
            {
            }
        }

        std::array<std::atomic<float>, kMaxChannels> inputPeak, outputPeak;
        std::atomic<float> clipOvershoot;
    };

    //==============================================================================================
    /** UI-side meter ballistics: instant attack, constant dB/second release, and a peak-hold marker
        that holds for a fixed time and then falls. Pure logic (no JUCE GUI) so it can be unit tested. */
    class MeterBallistics
    {
    public:
        static constexpr float kFloorDb = -100.0f;

        void setTimes (float releaseDbPerSecond, float holdSeconds) noexcept
        {
            release = releaseDbPerSecond;
            holdTime = holdSeconds;
        }

        void reset() noexcept
        {
            levelDb = kFloorDb;
            holdDb = kFloorDb;
            holdRemaining = 0.0f;
            maxDb = kFloorDb;
        }

        /** Feeds the linear block peak accumulated since the previous call. */
        void update (float linearPeak, float dtSeconds) noexcept
        {
            const float peakDb = linearPeak > 1.0e-5f ? 20.0f * std::log10 (linearPeak) : kFloorDb;

            levelDb = peakDb >= levelDb ? peakDb : std::max (peakDb, levelDb - release * dtSeconds);

            if (peakDb >= holdDb)
            {
                holdDb = peakDb;
                holdRemaining = holdTime;
            }
            else if (holdRemaining > 0.0f)
            {
                holdRemaining -= dtSeconds;
            }
            else
            {
                holdDb = std::max (peakDb, holdDb - 20.0f * dtSeconds);
            }

            maxDb = std::max (maxDb, peakDb);
        }

        void resetHold() noexcept { holdDb = levelDb; holdRemaining = 0.0f; maxDb = levelDb; }

        float getLevelDb() const noexcept { return levelDb; }
        float getHoldDb()  const noexcept { return holdDb; }
        /** Highest value seen since the last resetHold() (the "session" peak shown as a number). */
        float getMaxDb()   const noexcept { return maxDb; }

    private:
        float release = 24.0f;      // dB per second
        float holdTime = 1.5f;      // seconds
        float levelDb = kFloorDb;
        float holdDb = kFloorDb;
        float holdRemaining = 0.0f;
        float maxDb = kFloorDb;
    };
}
