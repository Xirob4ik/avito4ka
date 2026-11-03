// Loads the built Clipline.vst3 the way a DAW does (through the VST3 interfaces, via JUCE's VST3 hosting code)
// and checks the things a host relies on. Exit code 0 = every check passed.
//
//   ClipperHostCheck <path/to/Clipline.vst3> [--editor-seconds N]

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <iostream>

namespace
{
    int failures = 0;

    void check (bool ok, const juce::String& what)
    {
        std::cout << (ok ? "  [ ok ] " : "  [FAIL] ") << what << std::endl;
        if (! ok) ++failures;
    }

    // VST3 has numeric parameter IDs only (JUCE derives them from the string IDs), so the host finds the
    // parameters by their names, as a DAW's automation list shows them.
    juce::AudioProcessorParameter* findParam (juce::AudioPluginInstance& p, const juce::String& id)
    {
        static const juce::StringPairArray names = []
        {
            juce::StringPairArray n;
            n.set ("input", "Input Gain");     n.set ("clip", "Clip Threshold");  n.set ("output", "Output Gain");
            n.set ("mix", "Dry/Wet Mix");      n.set ("mode", "Clipping Mode");   n.set ("os", "Oversampling");
            n.set ("link", "Stereo Link");     n.set ("ceiling", "True Ceiling");
            return n;
        }();
        for (auto* param : p.getParameters())
            if (param->getName (64) == names[id])
                return param;
        return nullptr;
    }

    void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

    float processSeconds (juce::AudioPluginInstance& p, double seconds, float amplitude, int seed)
    {
        juce::Random rng (seed);
        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;
        float peak = 0.0f;
        const int blocks = (int) (seconds * 48000.0 / 512.0);
        for (int b = 0; b < blocks; ++b)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 512; ++i)
                    buffer.setSample (ch, i, amplitude * (rng.nextFloat() * 2.0f - 1.0f));
            p.processBlock (buffer, midi);
            for (int ch = 0; ch < 2; ++ch)
                peak = std::max (peak, buffer.getMagnitude (ch, 0, 512));
        }
        return peak;
    }
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;

    if (argc < 2)
    {
        std::cerr << "usage: ClipperHostCheck <Clipline.vst3> [--editor-seconds N]" << std::endl;
        return 2;
    }

    const juce::String path = juce::String::fromUTF8 (argv[1]);
    int editorSeconds = 0;
    for (int i = 2; i + 1 < argc; ++i)
        if (juce::String (argv[i]) == "--editor-seconds")
            editorSeconds = juce::String (argv[i + 1]).getIntValue();

    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> types;
    std::cout << "scanning " << path << " ..." << std::endl;
    format.findAllTypesForFile (types, path);

    std::cout << "VST3 bundle: " << path << std::endl;
    check (types.size() == 1, "the bundle exposes exactly one plugin (found " + juce::String (types.size()) + ")");
    if (types.isEmpty())
        return 1;

    const auto& d = *types[0];
    std::cout << "  name=" << d.name << "  vendor=" << d.manufacturerName << "  version=" << d.version
              << "  category=" << d.category << "  ins=" << d.numInputChannels << " outs=" << d.numOutputChannels << std::endl;
    check (d.name == "Clipline", "plugin name");
    check (! d.isInstrument, "registered as an effect");
    check (d.category.containsIgnoreCase ("Fx"), "VST3 category contains Fx");

    juce::String error;
    std::cout << "creating an instance ..." << std::endl;
    auto plugin = format.createInstanceFromDescription (d, 48000.0, 512, error);
    check (plugin != nullptr, "instance created" + (error.isNotEmpty() ? " (" + error + ")" : juce::String()));
    if (plugin == nullptr)
        return 1;

    // ---- parameters as the host sees them ------------------------------------------------------------
    const juce::StringArray ids { "input", "clip", "output", "mix", "mode", "os", "link", "ceiling" };
    check (plugin->getParameters().size() >= ids.size(), juce::String (plugin->getParameters().size()) + " parameters exposed");
    for (const auto& id : ids)
        check (findParam (*plugin, id) != nullptr, "parameter '" + id + "' visible to the host");
    if (auto* clip = findParam (*plugin, "clip"))
        check (clip->getText (clip->getValue(), 32).contains ("-3.0"), "CLIP default shown as " + clip->getText (clip->getValue(), 32));

    // ---- audio ------------------------------------------------------------------------------------------
    std::cout << "processing ..." << std::endl;
    plugin->setPlayConfigDetails (2, 2, 48000.0, 512);
    plugin->prepareToPlay (48000.0, 512);
    pump (300);
    check (plugin->getLatencySamples() == 60, "latency reported for 4x: " + juce::String (plugin->getLatencySamples()) + " samples (expected 60)");

    const float peak = processSeconds (*plugin, 2.0, 1.0f, 1);
    const float ceiling = std::pow (10.0f, -3.0f / 20.0f);
    check (peak > 0.5f && peak <= ceiling * 1.000001f,
           "full-scale noise is clipped to the -3 dB ceiling (peak " + juce::String (juce::Decibels::gainToDecibels (peak), 2) + " dBFS)");

    // ---- latency change through the VST3 restartComponent path -------------------------------------------
    if (auto* os = findParam (*plugin, "os"))
    {
        const int expected[] = { 0, 49, 60, 65 };
        for (int index : { 3, 0, 1, 2 })
        {
            os->setValueNotifyingHost ((float) index / 3.0f);
            processSeconds (*plugin, 0.1, 0.1f, 2);
            pump (400);                                  // plugin timer -> setLatencySamples -> restartComponent
            check (plugin->getLatencySamples() == expected[index],
                   "after switching to " + juce::String (1 << index) + "x the host sees latency " + juce::String (plugin->getLatencySamples())
                   + " (expected " + juce::String (expected[index]) + ")");
        }
    }

    // ---- state ------------------------------------------------------------------------------------------
    if (auto* clip = findParam (*plugin, "clip"))
    {
        clip->setValueNotifyingHost (0.25f);
        juce::MemoryBlock state;
        plugin->getStateInformation (state);
        clip->setValueNotifyingHost (0.9f);
        plugin->setStateInformation (state.getData(), (int) state.getSize());
        pump (200);
        check (std::abs (clip->getValue() - 0.25f) < 0.01f, "state round trip restores CLIP (" + juce::String (clip->getValue(), 3) + ")");
        check (state.getSize() > 0, "state size " + juce::String ((int) state.getSize()) + " bytes");
    }

    std::cout << "opening the editor ..." << std::endl;
    // ---- editor ------------------------------------------------------------------------------------------
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (plugin->createEditorIfNeeded());
        check (editor != nullptr, "editor created");
        if (editor != nullptr)
        {
            std::cout << "  editor size " << editor->getWidth() << "x" << editor->getHeight() << std::endl;
            // The editor reopens at the size the user last chose (675..1440 px wide), always in the 900:560 ratio.
            check (editor->getWidth() >= 675 && editor->getWidth() <= 1440
                       && std::abs (editor->getHeight() - editor->getWidth() * 560 / 900) <= 1,
                   "editor size is within 675..1440 px and keeps the 900:560 aspect ratio");

            juce::DocumentWindow window ("Clipline host check", juce::Colours::black, 0);
            window.setUsingNativeTitleBar (true);
            window.setContentNonOwned (editor.get(), true);
            window.setTopLeftPosition (0, 0);
            window.setVisible (true);
            processSeconds (*plugin, 0.5, 1.0f, 3);
            pump (juce::jmax (500, editorSeconds * 1000));
            check (window.isShowing(), "editor window is showing");
            window.setContentNonOwned (nullptr, false);
            std::cout << "closing the editor ..." << std::endl;
            editor.reset();
            pump (200);
        }
    }

    std::cout << "releasing the plugin ..." << std::endl;
    plugin->releaseResources();
    plugin.reset();
    pump (200);

    std::cout << (failures == 0 ? "ALL HOST CHECKS PASSED" : juce::String (failures) + " HOST CHECK(S) FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
