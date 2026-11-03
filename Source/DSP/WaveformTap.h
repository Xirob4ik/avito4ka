#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace clipline::dsp
{
    /** Lock-free feed of the *real* audio to the waveform display.

        The audio thread condenses the signal into fixed-duration "columns" (about 0.06 ms each, i.e. 3 samples at 48 kHz) holding
        the min/max of the clipper's input and output, and appends them to a ring. The UI thread copies
        the most recent columns whenever it repaints and decimates them to screen pixels itself, so the
        audio thread does almost no work (a few comparisons per sample) and the UI never blocks it.

        Single writer (audio thread), single reader (UI thread). The ring is far larger than the longest
        display window, so a reader that copies a recent span can never be lapped by the writer while it
        copies. All stored values are relaxed atomics, which compile to plain loads/stores but keep the
        concurrent access well defined. */
    class WaveformTap
    {
    public:
        static constexpr int kCapacity = 16384;                // columns, power of two
        static constexpr double kColumnSeconds = 0.0000625;    // 62.5 us

        struct Column
        {
            float inMin = 0.0f, inMax = 0.0f;     // signal entering the clipper (after INPUT gain)
            float outMin = 0.0f, outMax = 0.0f;   // clipper output (before MIX / OUTPUT gain)
        };

        WaveformTap() { reset(); }

        /** Not real-time: call while the audio thread is stopped (prepareToPlay). */
        void prepare (double sampleRate)
        {
            samplesPerColumn = std::max (1, (int) std::lround (sampleRate * kColumnSeconds));
            columnSeconds.store ((float) ((double) samplesPerColumn / sampleRate), std::memory_order_relaxed);
            resetAccumulator();
        }

        void reset() noexcept
        {
            for (auto& v : data) v.store (0.0f, std::memory_order_relaxed);
            writeCount.store (0, std::memory_order_relaxed);
            resetAccumulator();
        }

        // ---- audio thread ----------------------------------------------------------------------
        /** Per-sample min/max across channels is computed by the caller. Arrays hold n entries. */
        void push (const float* inMin, const float* inMax,
                   const float* outMin, const float* outMax, int n) noexcept
        {
            for (int i = 0; i < n; ++i)
            {
                accInMin  = std::min (accInMin,  inMin[i]);
                accInMax  = std::max (accInMax,  inMax[i]);
                accOutMin = std::min (accOutMin, outMin[i]);
                accOutMax = std::max (accOutMax, outMax[i]);

                if (++accCount >= samplesPerColumn)
                {
                    commitColumn();
                    resetAccumulator();
                }
            }
        }

        // ---- UI thread ---------------------------------------------------------------------------
        float getColumnSeconds() const noexcept { return columnSeconds.load (std::memory_order_relaxed); }

        std::uint64_t getWriteCount() const noexcept { return writeCount.load (std::memory_order_acquire); }

        /** Copies up to `maxColumns` of the newest columns, oldest first. Returns how many were copied and
            reports the absolute index one past the newest copied column (for grid alignment). */
        int copyLatest (Column* dest, int maxColumns, std::uint64_t& endIndex) const noexcept
        {
            const std::uint64_t end = getWriteCount();
            const int count = (int) std::min<std::uint64_t> ((std::uint64_t) std::min (maxColumns, kCapacity / 2), end);
            const std::uint64_t first = end - (std::uint64_t) count;

            for (int i = 0; i < count; ++i)
            {
                const std::size_t base = (std::size_t) ((first + (std::uint64_t) i) & (kCapacity - 1)) * 4;
                dest[i].inMin  = data[base + 0].load (std::memory_order_relaxed);
                dest[i].inMax  = data[base + 1].load (std::memory_order_relaxed);
                dest[i].outMin = data[base + 2].load (std::memory_order_relaxed);
                dest[i].outMax = data[base + 3].load (std::memory_order_relaxed);
            }

            endIndex = end;
            return count;
        }

    private:
        void resetAccumulator() noexcept
        {
            accInMin = accOutMin = 1.0e30f;
            accInMax = accOutMax = -1.0e30f;
            accCount = 0;
        }

        void commitColumn() noexcept
        {
            const std::uint64_t w = writeCount.load (std::memory_order_relaxed);
            const std::size_t base = (std::size_t) (w & (kCapacity - 1)) * 4;
            data[base + 0].store (accInMin,  std::memory_order_relaxed);
            data[base + 1].store (accInMax,  std::memory_order_relaxed);
            data[base + 2].store (accOutMin, std::memory_order_relaxed);
            data[base + 3].store (accOutMax, std::memory_order_relaxed);
            writeCount.store (w + 1, std::memory_order_release);
        }

        std::array<std::atomic<float>, (std::size_t) kCapacity * 4> data;
        std::atomic<std::uint64_t> writeCount { 0 };
        std::atomic<float> columnSeconds { (float) kColumnSeconds };

        // audio-thread-only accumulator
        int samplesPerColumn = 3;
        int accCount = 0;
        float accInMin = 1.0e30f, accInMax = -1.0e30f, accOutMin = 1.0e30f, accOutMax = -1.0e30f;
    };
}
