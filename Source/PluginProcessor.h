#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>

#include "DSP/ClipperEngine.h"
#include "Parameters.h"
#include "Presets/PresetManager.h"

/** The plugin's audio processor: owns the parameter tree and the (UI independent) ClipperEngine.

    Threading:
      - processBlock() runs on the audio thread. It reads parameters through lock-free atomics, calls the
        engine and publishes meter / waveform data through lock-free structures. It never allocates,
        locks or touches the editor.
      - Latency changes are *published* by the audio thread through an atomic and *forwarded to the host*
        by a message-thread timer, because AudioProcessor::setLatencySamples() takes a lock. */
class ClipperAudioProcessor : public juce::AudioProcessor,
                              private juce::Timer
{
public:
    ClipperAudioProcessor();
    ~ClipperAudioProcessor() override;

    //==============================================================================================
    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using juce::AudioProcessor::processBlock;           // keep the double-precision overloads visible
    using juce::AudioProcessor::processBlockBypassed;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Clipline"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // Presets are managed by clipline::PresetManager, not exposed as host "programs" (that would add an
    // extra automatable parameter to every host's list).
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================================
    /** Reads the current parameter values (lock-free; safe on any thread). */
    clipline::dsp::EngineSettings readSettings() const noexcept;

    clipline::dsp::ClipperEngine& getEngine() noexcept { return engine; }

    /** The latency the audio thread currently runs with, and the value last handed to the host. */
    int getPublishedLatencySamples() const noexcept { return publishedLatency.load (std::memory_order_relaxed); }

    /** Forwards a pending latency change to the host. Called by the timer; public so tests can call it. */
    void syncLatencyToHost();

    juce::AudioProcessorValueTreeState apvts;
    clipline::PresetManager presetManager;      // after apvts: it works on the parameter tree

    static constexpr int kStateVersion = 1;

private:
    void timerCallback() override { syncLatencyToHost(); }
    void processInternal (juce::AudioBuffer<float>&, const clipline::dsp::EngineSettings&) noexcept;

    clipline::dsp::ClipperEngine engine;

    std::atomic<float>* inputParam = nullptr;
    std::atomic<float>* clipParam = nullptr;
    std::atomic<float>* outputParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* modeParam = nullptr;
    std::atomic<float>* osParam = nullptr;
    std::atomic<float>* linkParam = nullptr;
    std::atomic<float>* ceilingParam = nullptr;

    std::atomic<int> publishedLatency { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipperAudioProcessor)
};
