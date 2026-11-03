#include "PluginProcessor.h"

#include "PluginEditor.h"

namespace ids = clipline::params;
using clipline::dsp::ClipMode;
using clipline::dsp::EngineSettings;

ClipperAudioProcessor::ClipperAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Clipline", ids::createLayout()),
      presetManager (apvts)
{
    inputParam  = apvts.getRawParameterValue (ids::kInput);
    clipParam   = apvts.getRawParameterValue (ids::kClip);
    outputParam = apvts.getRawParameterValue (ids::kOutput);
    mixParam    = apvts.getRawParameterValue (ids::kMix);
    modeParam   = apvts.getRawParameterValue (ids::kMode);
    osParam     = apvts.getRawParameterValue (ids::kOversampling);
    linkParam   = apvts.getRawParameterValue (ids::kLink);
    ceilingParam = apvts.getRawParameterValue (ids::kCeiling);

    jassert (inputParam && clipParam && outputParam && mixParam && modeParam && osParam && linkParam && ceilingParam);

    // Report the latency of the initial oversampling mode before the host ever calls prepareToPlay.
    engine.prepare (44100.0, 512, 2);
    publishedLatency.store (engine.getLatencySamplesFor (readSettings().oversamplingIndex));
    setLatencySamples (publishedLatency.load());

    startTimerHz (10);
}

ClipperAudioProcessor::~ClipperAudioProcessor()
{
    stopTimer();
}

//==================================================================================================
bool ClipperAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    return in == out && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

EngineSettings ClipperAudioProcessor::readSettings() const noexcept
{
    EngineSettings s;
    s.inputDb  = inputParam->load (std::memory_order_relaxed);
    s.clipDb   = clipParam->load (std::memory_order_relaxed);
    s.outputDb = outputParam->load (std::memory_order_relaxed);
    s.mixPercent = mixParam->load (std::memory_order_relaxed);
    s.mode = (ClipMode) juce::jlimit (0, ids::kNumModes - 1, (int) std::lround (modeParam->load (std::memory_order_relaxed)));
    s.oversamplingIndex = juce::jlimit (0, ids::kNumOversampling - 1, (int) std::lround (osParam->load (std::memory_order_relaxed)));
    s.link = linkParam->load (std::memory_order_relaxed) > 0.5f;
    s.ceiling = ceilingParam->load (std::memory_order_relaxed) > 0.5f;
    return s;
}

void ClipperAudioProcessor::prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock)
{
    const int numChannels = juce::jmax (1, getMainBusNumInputChannels());
    engine.prepare (sampleRate, juce::jmax (1, maximumExpectedSamplesPerBlock), numChannels);

    publishedLatency.store (engine.getLatencySamplesFor (readSettings().oversamplingIndex));
    setLatencySamples (publishedLatency.load());
}

void ClipperAudioProcessor::releaseResources()
{
    engine.reset();
}

//==================================================================================================
void ClipperAudioProcessor::processInternal (juce::AudioBuffer<float>& buffer, const EngineSettings& settings) noexcept
{
    const int numSamples = buffer.getNumSamples();

    // Outputs without a matching input (not possible with our layouts, but cheap to be safe).
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    engine.process (buffer, settings);
    publishedLatency.store (engine.getLatencySamples(), std::memory_order_relaxed);
}

void ClipperAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    processInternal (buffer, readSettings());
}

void ClipperAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // Bypass keeps the signal path (and its latency) intact and only turns the processing off: unity
    // gains and MIX = 0, reached through the usual smoothing ramps so engaging bypass never clicks and
    // never shifts the timing of the audio.
    juce::ScopedNoDenormals noDenormals;

    auto s = readSettings();
    s.inputDb = 0.0f;
    s.outputDb = 0.0f;
    s.mixPercent = 0.0f;
    processInternal (buffer, s);
}

void ClipperAudioProcessor::syncLatencyToHost()
{
    const int wanted = publishedLatency.load (std::memory_order_relaxed);
    if (wanted != getLatencySamples())
        setLatencySamples (wanted);
}

//==================================================================================================
juce::AudioProcessorEditor* ClipperAudioProcessor::createEditor()
{
    return new ClipperAudioProcessorEditor (*this);
}

void ClipperAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("stateVersion", kStateVersion, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ClipperAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}
