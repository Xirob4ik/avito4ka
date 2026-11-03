#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace clipline::dsp
{
    /** A multi-channel ring buffer written block by block and read back at arbitrary integer delays.

        Used to keep the dry path (and the display tap) sample-aligned with the oversampled wet path.
        Every latency the engine can have is just a different read tap into the same ring, so switching
        oversampling modes never has to move or refill the dry history.

        Real-time safe once prepared: no allocation in push() / read() / readHistory(). */
    class DelayLine
    {
    public:
        /** @param maxHistory  the largest delay or history length that will ever be requested. */
        void prepare (int numChannels, int maxHistory, int maxBlockSize)
        {
            const int needed = maxHistory + maxBlockSize + 1;
            int size = 16;
            while (size < needed)
                size <<= 1;

            ring.setSize (numChannels, size, false, true, false);
            ring.clear();
            mask       = size - 1;
            writePos   = 0;
            maxHistory_ = maxHistory;
        }

        void reset()
        {
            ring.clear();
            writePos = 0;
        }

        /** Appends numSamples samples to each of the first numChannels channels. */
        void push (const float* const* source, int numChannels, int numSamples) noexcept
        {
            const int chans = juce::jmin (numChannels, ring.getNumChannels());
            for (int ch = 0; ch < chans; ++ch)
            {
                float* dst = ring.getWritePointer (ch);
                const float* src = source[ch];
                int pos = writePos;
                for (int i = 0; i < numSamples; ++i)
                {
                    dst[pos] = src[i];
                    pos = (pos + 1) & mask;
                }
            }
            writePos = (writePos + numSamples) & mask;
        }

        /** Call after push(numSamples). Fills dest[i] with the sample that was pushed `delay` samples
            before the i-th sample of the block that was just pushed (delay = 0 returns that block). */
        void read (int channel, float* dest, int numSamples, int delay) const noexcept
        {
            jassert (delay >= 0 && delay <= maxHistory_);
            const float* src = ring.getReadPointer (channel);
            int pos = (writePos - numSamples - delay) & mask;
            for (int i = 0; i < numSamples; ++i)
            {
                dest[i] = src[pos];
                pos = (pos + 1) & mask;
            }
        }

        /** Copies the `count` newest samples already in the ring (oldest first). Call it *before* pushing
            the block that is about to be processed to get the history that precedes that block. */
        void readHistory (int channel, float* dest, int count) const noexcept
        {
            jassert (count >= 0 && count <= maxHistory_);
            const float* src = ring.getReadPointer (channel);
            int pos = (writePos - count) & mask;
            for (int i = 0; i < count; ++i)
            {
                dest[i] = src[pos];
                pos = (pos + 1) & mask;
            }
        }

        int getNumChannels() const noexcept { return ring.getNumChannels(); }

    private:
        juce::AudioBuffer<float> ring;
        int mask = 0;
        int writePos = 0;
        int maxHistory_ = 0;
    };
}
