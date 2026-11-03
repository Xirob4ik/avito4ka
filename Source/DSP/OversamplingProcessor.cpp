#include "OversamplingProcessor.h"

#include <cmath>

namespace clipline::dsp
{
    using Design = OversamplingProcessor::StageDesign;

    const std::array<Design, OversamplingProcessor::kNumStages>& OversamplingProcessor::stageDesigns() noexcept
    {
        // Starting point: JUCE's "maximum quality" half-band FIR parameters (first stage has half the
        // transition width because it runs closest to the audio band). The values were then nudged by
        // an offline search (ClipperTests --design-search) until every chain length gives an exact
        // integer latency. The "oversampling latency is integer" unit test guards this table.
        static const std::array<Design, kNumStages> table {{
            { 0.05f, -90.0f, 0.06f, -75.0f },
            { 0.10f, -81.0f, 0.12f, -65.0f },
            { 0.10f, -75.0f, 0.12f, -55.0f },
        }};
        return table;
    }

    int OversamplingProcessor::designLatencyEighths (const std::array<Design, kNumStages>& designs, int numStages)
    {
        using FD = juce::dsp::FilterDesign<float>;

        int eighths = 0;
        for (int n = 0; n < numStages; ++n)
        {
            const auto& d = designs[(size_t) n];
            const auto up   = FD::designFIRLowpassHalfBandEquirippleMethod (d.transitionUp,   d.stopbandUpDb);
            const auto down = FD::designFIRLowpassHalfBandEquirippleMethod (d.transitionDown, d.stopbandDownDb);

            // JUCE: stage latency = (orderUp + orderDown) / 2 samples at the stage's output rate,
            // which is 2^(n+1) times the base rate.
            const int stageLatency = (int) (up->getFilterOrder() + down->getFilterOrder()) / 2;
            eighths += stageLatency * (4 >> n);      // 8 / 2^(n+1)
        }
        return eighths;
    }

    void OversamplingProcessor::prepare (int numChannels, int maxBlockSize)
    {
        using Chain = juce::dsp::Oversampling<float>;
        const auto& designs = stageDesigns();

        for (int index = 0; index < kNumModes; ++index)
        {
            auto chain = std::make_unique<Chain> ((size_t) numChannels);
            chain->clearOversamplingStages();

            if (index == 0)
            {
                chain->addDummyOversamplingStage();     // 1x: plain copy, zero latency
            }
            else
            {
                for (int stage = 0; stage < index; ++stage)
                {
                    const auto& d = designs[(size_t) stage];
                    chain->addOversamplingStage (Chain::filterHalfBandFIREquiripple,
                                                 d.transitionUp,   d.stopbandUpDb,
                                                 d.transitionDown, d.stopbandDownDb);
                }
            }

            chain->initProcessing ((size_t) maxBlockSize);

            const auto latency = chain->getLatencyInSamples();
            latencies[(size_t) index] = (int) std::lround (latency);
            jassert (std::abs (latency - std::round (latency)) < 1.0e-4f);   // must be an exact integer

            chains[(size_t) index] = std::move (chain);
        }
    }

    void OversamplingProcessor::reset (int index) noexcept
    {
        if (auto& chain = chains[(size_t) clampIndex (index)])
            chain->reset();
    }

    juce::dsp::AudioBlock<float> OversamplingProcessor::processUp (int index, const juce::dsp::AudioBlock<const float>& input) noexcept
    {
        return chains[(size_t) clampIndex (index)]->processSamplesUp (input);
    }

    void OversamplingProcessor::processDown (int index, juce::dsp::AudioBlock<float>& output) noexcept
    {
        chains[(size_t) clampIndex (index)]->processSamplesDown (output);
    }
}
