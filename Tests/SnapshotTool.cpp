// Renders the real plugin editor to a PNG so the interface can be reviewed without a DAW.
//
//   ClipperTests --snapshot out.png [--preset "Punchy Drums"] [--width 900] [--scale 2]
//                                   [--material drums|bass|mix] [--os 0..3] [--hover clip|wave] [--software]
//
// The processor is fed synthetic material through its real processBlock(), then the editor is built
// and driven exactly like the timer would drive it, so meters and the waveform show real engine output.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <iostream>

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "TestUtil.h"

namespace
{
    juce::String argAfter (const juce::StringArray& args, const juce::String& flag, const juce::String& fallback)
    {
        const int i = args.indexOf (flag);
        return i >= 0 && i + 1 < args.size() ? args[i + 1] : fallback;
    }
}

int runSnapshotTool (const juce::StringArray& args);   // called from Main.cpp

int runSnapshotTool (const juce::StringArray& args)
{
    const auto outPath = argAfter (args, "--snapshot", "snapshot.png");
    const auto presetName = argAfter (args, "--preset", "Punchy Drums");
    const auto material = argAfter (args, "--material", "drums");
    const int width = argAfter (args, "--width", "900").getIntValue();
    const float scale = argAfter (args, "--scale", "2").getFloatValue();
    const auto osArg = argAfter (args, "--os", "");
    const auto hover = argAfter (args, "--hover", "");

    constexpr double fs = 48000.0;
    auto processorOwner = std::make_unique<ClipperAudioProcessor>();
    auto& processor = *processorOwner;
    processor.setPlayConfigDetails (2, 2, fs, 512);
    processor.prepareToPlay (fs, 512);

    const int index = processor.presetManager.findByName (presetName);
    if (index < 0)
    {
        std::cerr << "unknown preset: " << presetName << std::endl;
        return 3;
    }
    processor.presetManager.apply (index);

    if (osArg.isNotEmpty())
        if (auto* p = processor.apvts.getParameter (clipline::params::kOversampling))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) osArg.getIntValue()));

    // 3 s of real audio through the real processor, in host-sized blocks. Stop right after a kick so the
    // waveform shows a transient that is being clipped.
    juce::AudioBuffer<float> source (2, (int) (fs * 3.2));
    cltest::renderMaterial (source, material, fs);
    const int total = (int) (fs * 3.0) + (int) (fs * 0.036);       // the kick at t = 3.0 s has just played
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> block (2, 512);

    std::unique_ptr<ClipperAudioProcessorEditor> editor;
    {
        const juce::SharedResourcePointer<clipline::ui::fonts::FontLibrary> fonts;     // keep the fonts alive during setup
        editor.reset (static_cast<ClipperAudioProcessorEditor*> (processor.createEditor()));
    }

    for (int pos = 0; pos + 512 <= total; pos += 512)
    {
        for (int ch = 0; ch < 2; ++ch)
            block.copyFrom (ch, 0, source, ch, pos, 512);
        processor.processBlock (block, midi);
        if (pos % (512 * 4) == 0)
            editor->updateFromEngine (512.0 * 4.0 / fs);
    }
    editor->updateFromEngine (0.016);

    editor->setSize (width, (int) std::lround (width * 560.0 / 900.0));

    if (hover == "clip")
    {
        // emulate the cursor resting on the CLIP knob
        auto& knob = editor->getClipKnob();
        const juce::MouseEvent e (juce::Desktop::getInstance().getMainMouseSource(), knob.getDialBounds().getCentre(),
                                  {}, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &knob, &knob, juce::Time::getCurrentTime(),
                                  knob.getDialBounds().getCentre(), juce::Time::getCurrentTime(), 0, false);
        knob.mouseEnter (e);
    }

    // --software renders with JUCE's own software rasteriser instead of the platform renderer (Direct2D on
    // Windows); useful to tell platform-renderer differences (e.g. under Wine) from problems in our drawing.
    juce::Image image;
    if (args.contains ("--software"))
    {
        image = juce::Image (juce::Image::ARGB, juce::roundToInt ((float) editor->getWidth() * scale),
                             juce::roundToInt ((float) editor->getHeight() * scale), true, juce::SoftwareImageType());
        juce::Graphics g (image);
        g.addTransform (juce::AffineTransform::scale (scale));
        editor->paintEntireComponent (g, true);
    }
    else
    {
        image = editor->createComponentSnapshot (editor->getLocalBounds(), true, scale);
    }
    juce::File out (outPath);
    out.deleteFile();
    juce::FileOutputStream stream (out);
    if (! stream.openedOk())
    {
        std::cerr << "cannot write " << outPath << std::endl;
        return 4;
    }

    juce::PNGImageFormat png;
    png.writeImageToStream (image, stream);
    std::cout << "wrote " << outPath << " (" << image.getWidth() << "x" << image.getHeight() << ")" << std::endl;
    return 0;
}
