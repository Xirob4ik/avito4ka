#include <juce_audio_processors/juce_audio_processors.h>

#include "ParameterIds.h"
#include "PluginProcessor.h"
#include "Presets/FactoryPresets.h"
#include "TestUtil.h"

namespace ids = clipline::params;
using namespace cltest;

namespace
{
    constexpr double kFs = 48000.0;

    void setPlain (ClipperAudioProcessor& p, const char* id, float plain)
    {
        auto* param = p.apvts.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (plain));
    }

    float getPlain (ClipperAudioProcessor& p, const char* id)
    {
        auto* param = p.apvts.getParameter (id);
        return param->convertFrom0to1 (param->getValue());
    }

    void prepare (ClipperAudioProcessor& p, int channels, int block = 512)
    {
        juce::AudioProcessor::BusesLayout layout;
        const auto set = channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
        layout.inputBuses.add (set);
        layout.outputBuses.add (set);
        p.setBusesLayout (layout);
        p.setPlayConfigDetails (channels, channels, kFs, block);
        p.prepareToPlay (kFs, block);
    }

    juce::AudioBuffer<float> run (ClipperAudioProcessor& p, juce::AudioBuffer<float> in, int block, bool bypassed = false)
    {
        juce::MidiBuffer midi;
        for (int pos = 0; pos < in.getNumSamples(); pos += block)
        {
            const int n = std::min (block, in.getNumSamples() - pos);
            juce::AudioBuffer<float> view (in.getArrayOfWritePointers(), in.getNumChannels(), pos, n);
            if (bypassed) p.processBlockBypassed (view, midi); else p.processBlock (view, midi);
        }
        return in;
    }
}

//==================================================================================================
struct ParameterTests : public juce::UnitTest
{
    ParameterTests() : juce::UnitTest ("Parameters", "Clipline-Plugin") {}

    void runTest() override
    {
        auto pOwner = std::make_unique<ClipperAudioProcessor>();   // heap: the processor is large (waveform buffer)
        auto& p = *pOwner;

        beginTest ("exactly the eight documented parameters exist, with stable IDs, all automatable");
        {
            const char* expected[] = { ids::kInput, ids::kClip, ids::kOutput, ids::kMix, ids::kMode, ids::kOversampling, ids::kLink, ids::kCeiling };
            expectEquals (p.getParameters().size(), 8);
            for (const char* id : expected)
            {
                auto* param = p.apvts.getParameter (id);
                expect (param != nullptr, juce::String ("parameter exists: ") + id);
                if (param != nullptr)
                    expect (param->isAutomatable(), juce::String (id) + " is automatable");
            }

            // These strings are part of the project file format. If this test fails, a rename would
            // silently break existing DAW projects and automation.
            expectEquals (juce::String (ids::kInput), juce::String ("input"));
            expectEquals (juce::String (ids::kClip), juce::String ("clip"));
            expectEquals (juce::String (ids::kOutput), juce::String ("output"));
            expectEquals (juce::String (ids::kMix), juce::String ("mix"));
            expectEquals (juce::String (ids::kMode), juce::String ("mode"));
            expectEquals (juce::String (ids::kOversampling), juce::String ("os"));
            expectEquals (juce::String (ids::kLink), juce::String ("link"));
            expectEquals (juce::String (ids::kCeiling), juce::String ("ceiling"));
        }

        beginTest ("ranges and defaults");
        {
            auto range = [&] (const char* id) { return p.apvts.getParameter (id)->getNormalisableRange(); };
            expectEquals (range (ids::kInput).start, -24.0f);   expectEquals (range (ids::kInput).end, 24.0f);
            expectEquals (range (ids::kOutput).start, -24.0f);  expectEquals (range (ids::kOutput).end, 24.0f);
            expectEquals (range (ids::kClip).start, -24.0f);    expectEquals (range (ids::kClip).end, 0.0f);
            expectEquals (range (ids::kMix).start, 0.0f);       expectEquals (range (ids::kMix).end, 100.0f);

            auto def = [&] (const char* id) { auto* x = p.apvts.getParameter (id); return x->convertFrom0to1 (x->getDefaultValue()); };
            expectWithinAbsoluteError (def (ids::kInput), 0.0f, 1.0e-4f);
            expectWithinAbsoluteError (def (ids::kClip), -3.0f, 1.0e-4f);
            expectWithinAbsoluteError (def (ids::kOutput), 0.0f, 1.0e-4f);
            expectWithinAbsoluteError (def (ids::kMix), 100.0f, 1.0e-4f);
            expectWithinAbsoluteError (def (ids::kMode), 0.0f, 1.0e-4f);
            expectWithinAbsoluteError (def (ids::kOversampling), 2.0f, 1.0e-4f);
            expectWithinAbsoluteError (def (ids::kLink), 1.0f, 1.0e-4f);
            expectWithinAbsoluteError (def (ids::kCeiling), 1.0f, 1.0e-4f);

            auto* mode = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter (ids::kMode));
            auto* os   = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter (ids::kOversampling));
            expect (mode != nullptr && os != nullptr);
            expectEquals (mode->choices.joinIntoString ("|"), juce::String ("Hard|Soft|Cubic|Wavefold"));
            expectEquals (os->choices.joinIntoString ("|"), juce::String ("1x|2x|4x|8x"));
        }

        beginTest ("text conversion shown by the host");
        {
            auto* input = p.apvts.getParameter (ids::kInput);
            expectEquals (input->getText (input->convertTo0to1 (3.0f), 0), juce::String ("+3.0 dB"));
            expectEquals (input->getText (input->convertTo0to1 (-6.5f), 0), juce::String ("-6.5 dB"));
            expectWithinAbsoluteError (input->convertFrom0to1 (input->getValueForText ("+4.5 dB")), 4.5f, 1.0e-4f);
            expectWithinAbsoluteError (input->convertFrom0to1 (input->getValueForText ("-7,5")), -7.5f, 1.0e-4f);

            auto* mix = p.apvts.getParameter (ids::kMix);
            expectEquals (mix->getText (mix->convertTo0to1 (100.0f), 0), juce::String ("100%"));
            expectEquals (mix->getText (mix->convertTo0to1 (37.5f), 0), juce::String ("37.5%"));
        }
    }
};
static ParameterTests parameterTests;

//==================================================================================================
struct StateTests : public juce::UnitTest
{
    StateTests() : juce::UnitTest ("DAW state", "Clipline-Plugin") {}

    void runTest() override
    {
        beginTest ("state round-trips every parameter and the preset name");
        {
            auto aOwner = std::make_unique<ClipperAudioProcessor>();
            auto& a = *aOwner;
            setPlain (a, ids::kInput, 7.3f);   setPlain (a, ids::kClip, -9.7f);  setPlain (a, ids::kOutput, -2.4f);
            setPlain (a, ids::kMix, 62.5f);    setPlain (a, ids::kMode, 2.0f);   setPlain (a, ids::kOversampling, 3.0f);
            setPlain (a, ids::kLink, 0.0f);    setPlain (a, ids::kCeiling, 0.0f);
            a.apvts.state.setProperty ("presetName", "Aggressive Drums", nullptr);

            juce::MemoryBlock blob;
            a.getStateInformation (blob);
            expectGreaterThan ((int) blob.getSize(), 0);

            auto bOwner = std::make_unique<ClipperAudioProcessor>();

            auto& b = *bOwner;
            b.setStateInformation (blob.getData(), (int) blob.getSize());
            for (const char* id : { ids::kInput, ids::kClip, ids::kOutput, ids::kMix, ids::kMode, ids::kOversampling, ids::kLink, ids::kCeiling })
                expectWithinAbsoluteError (getPlain (b, id), getPlain (a, id), 1.0e-4f, juce::String ("restored ") + id);
            expectEquals (b.presetManager.getCurrentName(), juce::String ("Aggressive Drums"));
        }

        beginTest ("restoring state while audio is prepared updates the DSP settings");
        {
            auto aOwner = std::make_unique<ClipperAudioProcessor>();
            auto& a = *aOwner;
            setPlain (a, ids::kClip, -12.0f);
            juce::MemoryBlock blob;
            a.getStateInformation (blob);

            auto bOwner = std::make_unique<ClipperAudioProcessor>();

            auto& b = *bOwner;
            prepare (b, 2);
            b.setStateInformation (blob.getData(), (int) blob.getSize());
            const auto s = b.readSettings();
            expectWithinAbsoluteError (s.clipDb, -12.0f, 1.0e-4f);
        }

        beginTest ("garbage, empty or foreign state is ignored without crashing or changing anything");
        {
            auto aOwner = std::make_unique<ClipperAudioProcessor>();
            auto& a = *aOwner;
            setPlain (a, ids::kClip, -5.0f);

            const char junk[] = "this is not a plugin state";
            a.setStateInformation (junk, (int) sizeof (junk));
            a.setStateInformation (junk, 0);
            a.setStateInformation (nullptr, 0);

            juce::XmlElement foreign ("SomethingElse");
            juce::MemoryBlock blob;
            ClipperAudioProcessor::copyXmlToBinary (foreign, blob);
            a.setStateInformation (blob.getData(), (int) blob.getSize());

            expectWithinAbsoluteError (getPlain (a, ids::kClip), -5.0f, 1.0e-4f);
        }

        beginTest ("state with out-of-range values is clamped into the legal range");
        {
            auto aOwner = std::make_unique<ClipperAudioProcessor>();
            auto& a = *aOwner;
            setPlain (a, ids::kClip, -5.0f);                    // makes sure the parameter has a node in the tree
            auto state = a.apvts.copyState();
            auto child = state.getChildWithProperty ("id", ids::kClip);
            expect (child.isValid(), "the clip parameter is stored in the state");
            child.setProperty ("value", 500.0f, nullptr);
            auto xml = state.createXml();
            juce::MemoryBlock blob;
            ClipperAudioProcessor::copyXmlToBinary (*xml, blob);

            auto bOwner = std::make_unique<ClipperAudioProcessor>();

            auto& b = *bOwner;
            b.setStateInformation (blob.getData(), (int) blob.getSize());
            const auto s = b.readSettings();
            expectLessOrEqual (s.clipDb, 0.0f);
            expectGreaterOrEqual (s.clipDb, -24.0f);
        }
    }
};
static StateTests stateTests;

//==================================================================================================
struct ProcessingTests : public juce::UnitTest
{
    ProcessingTests() : juce::UnitTest ("Audio through the plugin", "Clipline-Plugin") {}

    void runTest() override
    {
        beginTest ("bus layouts: mono and stereo are accepted, everything else is refused");
        {
            auto pOwner = std::make_unique<ClipperAudioProcessor>();
            auto& p = *pOwner;
            auto layout = [] (juce::AudioChannelSet in, juce::AudioChannelSet out)
            {
                juce::AudioProcessor::BusesLayout l;
                l.inputBuses.add (in);
                l.outputBuses.add (out);
                return l;
            };
            expect (p.isBusesLayoutSupported (layout (juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo())));
            expect (p.isBusesLayoutSupported (layout (juce::AudioChannelSet::mono(), juce::AudioChannelSet::mono())));
            expect (! p.isBusesLayoutSupported (layout (juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo())));
            expect (! p.isBusesLayoutSupported (layout (juce::AudioChannelSet::create5point1(), juce::AudioChannelSet::create5point1())));
        }

        beginTest ("the plugin produces the same audio as the bare engine (stereo)");
        for (int os : { 0, 2 })
        {
            auto pOwner = std::make_unique<ClipperAudioProcessor>();
            auto& p = *pOwner;
            setPlain (p, ids::kInput, 5.0f);  setPlain (p, ids::kClip, -8.0f);  setPlain (p, ids::kOutput, 1.5f);
            setPlain (p, ids::kMix, 85.0f);   setPlain (p, ids::kMode, 1.0f);   setPlain (p, ids::kOversampling, (float) os);
            setPlain (p, ids::kLink, 0.0f);
            prepare (p, 2);

            const auto in = makeNoise (2, 24000, 0.6f, 3);
            const auto viaPlugin = run (p, in, 337);

            ClipperEngine engine;
            engine.prepare (kFs, 512, 2);
            const auto viaEngine = render (engine, in, p.readSettings(), 337);

            for (int ch = 0; ch < 2; ++ch)
                expectEquals (maxAbsDiff (viaPlugin, viaEngine, ch, 0, 24000), 0.0f, "channel " + juce::String (ch) + ", " + juce::String (os) + " os index");
            expectGreaterThan (maxAbsDiff (viaPlugin, in, 0, 0, 24000), 0.1f, "the signal really was processed");
        }

        beginTest ("mono instance matches the left channel of a stereo instance");
        {
            auto stereoOwner = std::make_unique<ClipperAudioProcessor>();
            auto& stereo = *stereoOwner;
            auto monoOwner = std::make_unique<ClipperAudioProcessor>();
            auto& mono = *monoOwner;
            for (auto* p : { &stereo, &mono })
            {
                setPlain (*p, ids::kInput, 8.0f);  setPlain (*p, ids::kClip, -6.0f);  setPlain (*p, ids::kOversampling, 1.0f);
            }
            prepare (stereo, 2);
            prepare (mono, 1);

            const auto noise = makeNoise (1, 12000, 0.7f, 9);
            juce::AudioBuffer<float> st (2, 12000);
            st.copyFrom (0, 0, noise, 0, 0, 12000);
            st.copyFrom (1, 0, noise, 0, 0, 12000);

            const auto a = run (stereo, st, 256);
            const auto b = run (mono, noise, 256);
            float worst = 0.0f;
            for (int i = 0; i < 12000; ++i)
                worst = std::max (worst, std::abs (a.getSample (0, i) - b.getSample (0, i)));
            expectLessThan (worst, 1.0e-6f);
            expect (allFinite (b));
        }

        beginTest ("parameter changes made during playback take effect");
        {
            auto pOwner = std::make_unique<ClipperAudioProcessor>();
            auto& p = *pOwner;
            setPlain (p, ids::kOversampling, 0.0f);  setPlain (p, ids::kClip, -12.0f);
            prepare (p, 2);
            const auto loud = makeSine (2, 4800, kFs, 440.0, 0.9f);

            setPlain (p, ids::kMix, 100.0f);
            run (p, loud, 480);                                  // warm-up with the first setting
            const auto wet = run (p, loud, 480);
            setPlain (p, ids::kMix, 0.0f);
            run (p, loud, 480);
            const auto dry = run (p, loud, 480);                 // smoothed to the new value by now

            expectLessThan (peakOf (wet, 0), 0.26f, "clipped at -12 dB (0.251)");
            expectGreaterThan (peakOf (dry, 0), 0.89f, "MIX = 0 passes the original level");
        }

        beginTest ("bypass keeps the latency (dry signal arrives delayed by exactly the reported latency)");
        for (int os : { 0, 1, 3 })
        {
            auto pOwner = std::make_unique<ClipperAudioProcessor>();
            auto& p = *pOwner;
            setPlain (p, ids::kOversampling, (float) os);  setPlain (p, ids::kInput, 12.0f);  setPlain (p, ids::kClip, -20.0f);
            prepare (p, 2);
            const int latency = p.getEngine().getLatencySamplesFor (os);

            const auto in = makeNoise (2, 8000, 0.8f, 5);
            auto out = run (p, in, 300, true);

            // The bypass ramp (20 ms) starts from the processed state; compare after it settled.
            const int settled = (int) (kFs * 0.05) + latency;
            float worst = 0.0f;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = settled; i < 8000; ++i)
                    worst = std::max (worst, std::abs (out.getSample (ch, i) - in.getSample (ch, i - latency)));
            expectLessThan (worst, 1.0e-5f, "oversampling index " + juce::String (os));
        }

        beginTest ("latency is reported to the host and follows the oversampling mode");
        {
            auto pOwner = std::make_unique<ClipperAudioProcessor>();
            auto& p = *pOwner;
            prepare (p, 2);
            expectEquals (p.getLatencySamples(), p.getEngine().getLatencySamplesFor (2), "initial (4x)");

            const int expectedLatency[] = { 0, 49, 60, 65 };
            for (int os : { 3, 0, 1, 2 })
            {
                setPlain (p, ids::kOversampling, (float) os);
                run (p, makeNoise (2, 2048, 0.2f), 256);           // the audio thread publishes the new latency
                p.syncLatencyToHost();                              // the message thread forwards it
                expectEquals (p.getLatencySamples(), expectedLatency[os], "oversampling index " + juce::String (os));
            }
        }

        beginTest ("hostile input: NaN / Inf / huge values never reach the host");
        {
            auto pOwner = std::make_unique<ClipperAudioProcessor>();
            auto& p = *pOwner;
            prepare (p, 2);
            auto in = makeNoise (2, 6000, 0.5f, 2);
            in.setSample (0, 100, std::numeric_limits<float>::quiet_NaN());
            in.setSample (1, 200, std::numeric_limits<float>::infinity());
            in.setSample (0, 300, -std::numeric_limits<float>::infinity());
            in.setSample (1, 400, 1.0e37f);
            expect (allFinite (run (p, in, 128)));
        }

        beginTest ("odd block sizes (1 sample, primes, larger than announced) give the same audio as 256-sample blocks");
        {
            const auto in = makeNoise (2, 20000, 0.6f, 12);

            auto refOwner = std::make_unique<ClipperAudioProcessor>();

            auto& ref = *refOwner;
            setPlain (ref, ids::kInput, 6.0f);  setPlain (ref, ids::kClip, -9.0f);
            prepare (ref, 2, 256);
            const auto reference = run (ref, in, 256);
            expect (allFinite (reference));

            for (int block : { 1, 13, 97, 1000, 20000 })
            {
                auto qOwner = std::make_unique<ClipperAudioProcessor>();
                auto& q = *qOwner;
                setPlain (q, ids::kInput, 6.0f);  setPlain (q, ids::kClip, -9.0f);
                prepare (q, 2, 256);                                 // announced maximum is 256, blocks may exceed it
                const auto out = run (q, in, block);
                expect (allFinite (out), "block " + juce::String (block));
                for (int ch = 0; ch < 2; ++ch)
                    expectLessThan (maxAbsDiff (out, reference, ch, 0, 20000), 1.0e-5f,
                                    "block " + juce::String (block) + " channel " + juce::String (ch));
            }
        }
    }
};
static ProcessingTests processingTests;

//==================================================================================================
struct PresetTests : public juce::UnitTest
{
    PresetTests() : juce::UnitTest ("Presets", "Clipline-Plugin") {}

    void runTest() override
    {
        namespace fp = clipline::presets;

        beginTest ("factory table: unique names, valid values, the presets the product promises exist");
        {
            juce::StringArray names;
            for (std::size_t i = 0; i < fp::getNumFactoryPresets(); ++i)
            {
                const auto& f = fp::getFactoryPreset (i);
                expect (! names.contains (f.name), juce::String ("unique: ") + f.name);
                names.add (f.name);
                expect (f.inputDb >= ids::kGainMinDb && f.inputDb <= ids::kGainMaxDb, juce::String (f.name) + " input");
                expect (f.outputDb >= ids::kGainMinDb && f.outputDb <= ids::kGainMaxDb, juce::String (f.name) + " output");
                expect (f.clipDb >= ids::kClipMinDb && f.clipDb <= ids::kClipMaxDb, juce::String (f.name) + " clip");
                expect (f.mixPercent >= 0.0f && f.mixPercent <= 100.0f, juce::String (f.name) + " mix");
                expect (f.mode >= 0 && f.mode < ids::kNumModes, juce::String (f.name) + " mode");
                expect (f.oversampling >= 0 && f.oversampling < ids::kNumOversampling, juce::String (f.name) + " os");
            }
            for (const char* required : { "Clean Peak Control", "Punchy Drums", "Aggressive Drums", "808 Control",
                                          "Hyperpop Smash", "Soft Saturation", "Mix Bus Clipper" })
                expect (names.contains (required), juce::String ("has preset ") + required);
        }

        beginTest ("every fully wet preset keeps realistic programme material below 0 dBFS (measured through the plugin)");
        for (std::size_t i = 0; i < fp::getNumFactoryPresets(); ++i)
        {
            const auto& f = fp::getFactoryPreset (i);
            if (f.mixPercent < 100.0f)
                continue;

            double worstDb = -200.0;
            juce::String worstKind;
            for (const char* kind : { "drums", "bass", "mix" })
            {
                auto pOwner = std::make_unique<ClipperAudioProcessor>();
                auto& p = *pOwner;
                p.presetManager.apply (p.presetManager.findByName (f.name));
                prepare (p, 2);

                juce::AudioBuffer<float> material (2, (int) (kFs * 2.0));
                renderMaterial (material, kind, kFs);
                const auto out = run (p, material, 512);          // the whole render is measured, nothing skipped
                const double peakDb = ampToDb ((double) std::max (peakOf (out, 0), peakOf (out, 1)));
                if (peakDb > worstDb) { worstDb = peakDb; worstKind = kind; }
            }
            logMessage (juce::String (f.name) + ": ceiling " + juce::String (f.clipDb + f.outputDb, 1)
                        + " dB, worst measured peak " + juce::String (worstDb, 2) + " dBFS (" + worstKind + ")");
            expectLessThan (worstDb, -0.05, juce::String (f.name) + " stays below full scale on drums / bass / mix material");
        }

        beginTest ("CLIP is a true ceiling: no overshoot in any mode at any oversampling factor");
        {
            // Without the engine's base-rate safety stage the down-sampling filters let clipped material ring
            // above the threshold (measured before that stage existed: ~1.5 dB for clipped tones above 10 kHz,
            // ~3 dB for full-scale noise, ~4 dB for clipped hi-hats). These signals are the worst cases found.
            constexpr int N = 48000;
            auto noiseAndSine = makeNoise (2, N, 1.0f, 77);
            const auto tone = makeSine (2, N, kFs, 3011.0, 1.0f);
            for (int ch = 0; ch < 2; ++ch)
                noiseAndSine.addFrom (ch, 0, tone, ch, 0, N);
            juce::AudioBuffer<float> drums (2, N);
            renderMaterial (drums, "drums", kFs);
            drums.applyGain (4.0f);

            std::vector<std::pair<juce::String, juce::AudioBuffer<float>>> signals;
            for (double f : { 1000.0, 12000.0, 17000.0, 20000.0 })
                signals.emplace_back ("sine " + juce::String ((int) f) + " Hz", makeSine (2, N, kFs, f, 1.0f));
            signals.emplace_back ("white noise", makeNoise (2, N, 1.0f, 5));
            signals.emplace_back ("noise + sine", noiseAndSine);
            signals.emplace_back ("drums +12 dB", drums);

            const double ceiling = std::pow (10.0, -3.0 / 20.0);
            for (int os = 0; os < 4; ++os)
                for (int mode = 0; mode < 4; ++mode)
                    for (bool link : { true, false })
                    {
                        double worst = 0.0;
                        for (auto& [label, sig] : signals)
                        {
                            EngineSettings s;
                            s.clipDb = -3.0f;
                            s.oversamplingIndex = os;
                            s.mode = (ClipMode) mode;
                            s.link = link;
                            const auto out = renderFresh (sig, s);
                            worst = std::max (worst, (double) std::max (peakOf (out, 0), peakOf (out, 1)) / ceiling);
                        }
                        expectLessOrEqual (worst, 1.0 + 1.0e-6, "os " + juce::String (os) + " mode " + juce::String (mode)
                                                                   + (link ? " linked" : " unlinked"));
                    }
        }

        beginTest ("applying a preset sets every parameter, names it, and clears the modified flag");
        {
            auto pOwner = std::make_unique<ClipperAudioProcessor>();
            auto& p = *pOwner;
            auto& mgr = p.presetManager;
            for (std::size_t i = 0; i < fp::getNumFactoryPresets(); ++i)
            {
                const auto& f = fp::getFactoryPreset (i);
                const int index = mgr.findByName (f.name);
                expect (index >= 0, juce::String ("listed: ") + f.name);
                expect (mgr.apply (index));

                expectWithinAbsoluteError (getPlain (p, ids::kInput),  f.inputDb,  0.06f, juce::String (f.name) + " input");
                expectWithinAbsoluteError (getPlain (p, ids::kClip),   f.clipDb,   0.06f, juce::String (f.name) + " clip");
                expectWithinAbsoluteError (getPlain (p, ids::kOutput), f.outputDb, 0.06f, juce::String (f.name) + " output");
                expectWithinAbsoluteError (getPlain (p, ids::kMix),    f.mixPercent, 0.06f, juce::String (f.name) + " mix");
                expectEquals ((int) std::lround (getPlain (p, ids::kMode)), f.mode, juce::String (f.name) + " mode");
                expectEquals ((int) std::lround (getPlain (p, ids::kOversampling)), f.oversampling, juce::String (f.name) + " os");
                expect ((getPlain (p, ids::kLink) > 0.5f) == f.link, juce::String (f.name) + " link");
                expectEquals (mgr.getCurrentName(), juce::String (f.name));
                expect (! mgr.isModified(), juce::String (f.name) + " not modified right after applying");
            }

            expect (mgr.apply (mgr.findByName ("Punchy Drums")));
            setPlain (p, ids::kClip, -17.0f);
            expect (mgr.isModified(), "turning a knob marks the preset as modified");
            expect (! mgr.apply (-1) && ! mgr.apply (9999), "invalid indices are refused");
        }

        beginTest ("step() walks the list and wraps around");
        {
            auto pOwner = std::make_unique<ClipperAudioProcessor>();
            auto& p = *pOwner;
            auto& mgr = p.presetManager;
            const int count = (int) mgr.getEntries().size();
            mgr.apply (0);
            mgr.step (-1);
            expectEquals (mgr.getCurrentIndex(), count - 1, "wraps backwards");
            mgr.step (+1);
            expectEquals (mgr.getCurrentIndex(), 0, "wraps forwards");
            mgr.step (+1);
            expectEquals (mgr.getCurrentIndex(), 1);
        }

        beginTest ("user presets: saved to disk, listed, restored; factory names and empty names refused");
        {
            auto pOwner = std::make_unique<ClipperAudioProcessor>();
            auto& p = *pOwner;
            auto& mgr = p.presetManager;
            const juce::String presetName = "Clipline unit test preset";
            const auto file = clipline::PresetManager::getUserPresetDirectory()
                                  .getChildFile (juce::File::createLegalFileName (presetName)).withFileExtension ("clpreset");
            file.deleteFile();

            setPlain (p, ids::kInput, 11.1f);  setPlain (p, ids::kClip, -14.3f);  setPlain (p, ids::kMix, 41.0f);
            setPlain (p, ids::kMode, 3.0f);    setPlain (p, ids::kOversampling, 0.0f);  setPlain (p, ids::kLink, 0.0f);

            expect (mgr.saveUserPreset (presetName));
            expect (file.existsAsFile(), "file written");
            expect (! mgr.isModified(), "the saved preset is the current one");

            auto qOwner = std::make_unique<ClipperAudioProcessor>();

            auto& q = *qOwner;                          // a fresh instance finds it on disk
            const int index = q.presetManager.findByName (presetName);
            expect (index >= 0, "listed after a rescan");
            if (index >= 0)
            {
                expect (q.presetManager.getEntries()[(size_t) index].isUser);
                q.presetManager.apply (index);
                expectWithinAbsoluteError (getPlain (q, ids::kInput), 11.1f, 0.06f);
                expectWithinAbsoluteError (getPlain (q, ids::kClip), -14.3f, 0.06f);
                expectWithinAbsoluteError (getPlain (q, ids::kMix), 41.0f, 0.06f);
                expectEquals ((int) std::lround (getPlain (q, ids::kMode)), 3);
                expectEquals ((int) std::lround (getPlain (q, ids::kOversampling)), 0);
                expect (getPlain (q, ids::kLink) < 0.5f);
            }

            expect (! mgr.saveUserPreset (""), "empty name refused");
            expect (! mgr.saveUserPreset ("   "), "blank name refused");
            expect (! mgr.saveUserPreset ("Punchy Drums"), "a user preset can not shadow a factory preset");
            file.deleteFile();
        }

        beginTest ("a hand-edited preset file with absurd values is clamped when read");
        {
            const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("clipline_bad_preset.clpreset");
            file.replaceWithText ("<ClipperPreset version=\"1\" name=\"Bad\" input=\"900\" clip=\"-900\" output=\"-900\" mix=\"900\" mode=\"77\" os=\"-3\" link=\"1\"/>");
            juce::String parsedName;
            clipline::PresetManager::Values v;
            expect (clipline::PresetManager::readPresetFile (file, parsedName, v));
            expectEquals (v.inputDb, 24.0f);   expectEquals (v.clipDb, -24.0f);   expectEquals (v.outputDb, -24.0f);
            expectEquals (v.mixPercent, 100.0f);   expectEquals (v.mode, 3);   expectEquals (v.oversampling, 0);

            file.replaceWithText ("not xml at all");
            expect (! clipline::PresetManager::readPresetFile (file, parsedName, v));
            file.deleteFile();
        }
    }
};
static PresetTests presetTests;
