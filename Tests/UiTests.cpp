// Interface tests: the real editor, driven with synthesised mouse events, checked against the parameters
// (which is what the host sees). Runs headless; the editor is never put on screen.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "ParameterIds.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "TestUtil.h"
#include "UI/Fonts.h"
#include "UI/UiSettings.h"

namespace ids = clipline::params;
using namespace cltest;

namespace
{
    float plain (ClipperAudioProcessor& p, const char* id)
    {
        auto* param = p.apvts.getParameter (id);
        return param->convertFrom0to1 (param->getValue());
    }

    /** Counts begin / end gesture notifications: the host needs every edit wrapped in one gesture. */
    struct GestureCounter : juce::AudioProcessorParameter::Listener
    {
        explicit GestureCounter (juce::AudioProcessorParameter& p) : param (p) { param.addListener (this); }
        ~GestureCounter() override { param.removeListener (this); }
        void parameterValueChanged (int, float) override { ++changes; }
        void parameterGestureChanged (int, bool starting) override { (starting ? begins : ends)++; }
        juce::AudioProcessorParameter& param;
        int begins = 0, ends = 0, changes = 0;
    };

    struct Mouse
    {
        explicit Mouse (juce::Component& comp) : c (comp) {}

        juce::Component& c;
        juce::Point<float> down;
        juce::Time downTime = juce::Time::getCurrentTime();

        juce::MouseEvent event (juce::Point<float> pos, juce::ModifierKeys mods = {}, int clicks = 1, bool dragged = false) const
        {
            return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), pos, mods, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                     &c, &c, juce::Time::getCurrentTime(), down, downTime, clicks, dragged);
        }

        void press (juce::Point<float> p, juce::ModifierKeys mods = juce::ModifierKeys::leftButtonModifier)
        {
            down = p;
            downTime = juce::Time::getCurrentTime();
            c.mouseDown (event (p, mods));
        }

        void dragTo (juce::Point<float> p, juce::ModifierKeys mods = juce::ModifierKeys::leftButtonModifier)
        {
            c.mouseDrag (event (p, mods, 1, true));
        }

        void release (juce::Point<float> p, bool dragged)
        {
            c.mouseUp (event (p, {}, 1, dragged));
        }
    };

    struct EditorFixture
    {
        // The editor remembers its size (and other choices) in the user's UI settings file. Tests must leave
        // the real user's settings exactly as they found them.
        struct SettingsGuard
        {
            int width = clipline::ui::settings::getEditorWidth();
            float windowMs = clipline::ui::settings::getWindowMs();
            bool tooltips = clipline::ui::settings::getTooltipsEnabled();
            bool software = clipline::ui::settings::getSoftwareRenderer();
            ~SettingsGuard()
            {
                clipline::ui::settings::setEditorWidth (width);
                clipline::ui::settings::setWindowMs (windowMs);
                clipline::ui::settings::setTooltipsEnabled (tooltips);
                clipline::ui::settings::setSoftwareRenderer (software);
            }
        } settingsGuard;            // declared first: restores after the editor is gone

        juce::SharedResourcePointer<clipline::ui::fonts::FontLibrary> fonts;
        ClipperAudioProcessor processor;
        std::unique_ptr<ClipperAudioProcessorEditor> editor;

        EditorFixture()
        {
            processor.setPlayConfigDetails (2, 2, 48000.0, 512);
            processor.prepareToPlay (48000.0, 512);
            editor.reset (static_cast<ClipperAudioProcessorEditor*> (processor.createEditor()));
            editor->setSize (900, 560);
        }

        ~EditorFixture()
        {
            editor.reset();
        }
    };
}

//==================================================================================================
struct KnobTests : public juce::UnitTest
{
    KnobTests() : juce::UnitTest ("Knobs", "Clipline-UI") {}

    void runTest() override
    {
        beginTest ("vertical drag: 220 px is the full range, wrapped in exactly one gesture");
        {
            EditorFixture f;
            auto& knob = f.editor->getInputKnob();
            GestureCounter gestures (knob.getParameter());
            Mouse m (knob);
            const auto c = knob.getDialBounds().getCentre();

            m.press (c);
            m.dragTo (c.translated (0.0f, -11.0f));
            m.dragTo (c.translated (0.0f, -22.0f));          // 22 px up = 10 % of 48 dB = +4.8 dB
            m.release (c.translated (0.0f, -22.0f), true);

            expectWithinAbsoluteError (plain (f.processor, ids::kInput), 4.8f, 0.051f);
            expectEquals (gestures.begins, 1);
            expectEquals (gestures.ends, 1);

            m.press (c);
            m.dragTo (c.translated (0.0f, 400.0f));          // far below the bottom: clamps at the minimum
            m.release (c.translated (0.0f, 400.0f), true);
            expectWithinAbsoluteError (plain (f.processor, ids::kInput), -24.0f, 0.001f);
        }

        beginTest ("Shift / Ctrl drag is ten times finer");
        {
            EditorFixture f;
            auto& knob = f.editor->getInputKnob();
            Mouse m (knob);
            const auto c = knob.getDialBounds().getCentre();
            m.press (c, juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::shiftModifier);
            m.dragTo (c.translated (0.0f, -22.0f), juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::shiftModifier);
            m.release (c.translated (0.0f, -22.0f), true);
            expectWithinAbsoluteError (plain (f.processor, ids::kInput), 0.5f, 0.051f);   // 0.48 dB on the 0.1 dB grid

            m.press (c, juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::ctrlModifier);
            m.dragTo (c.translated (0.0f, 22.0f), juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::ctrlModifier);
            m.release (c.translated (0.0f, 22.0f), true);
            expectWithinAbsoluteError (plain (f.processor, ids::kInput), 0.0f, 0.051f);
        }

        beginTest ("double-click resets to the default");
        {
            EditorFixture f;
            auto& knob = f.editor->getClipKnob();
            f.editor->getClipKnob().commitText ("-15");
            expectWithinAbsoluteError (plain (f.processor, ids::kClip), -15.0f, 0.001f);

            Mouse m (knob);
            const auto c = knob.getDialBounds().getCentre();
            knob.mouseDoubleClick (m.event (c, juce::ModifierKeys::leftButtonModifier, 2));
            expectWithinAbsoluteError (plain (f.processor, ids::kClip), ids::kClipDefaultDb, 0.001f);
        }

        beginTest ("mouse wheel steps the value, one gesture per notch");
        {
            EditorFixture f;
            auto& knob = f.editor->getMixKnob();
            GestureCounter gestures (knob.getParameter());
            Mouse m (knob);
            juce::MouseWheelDetails wheel {};
            wheel.deltaY = -0.2f;
            const float before = plain (f.processor, ids::kMix);
            knob.mouseWheelMove (m.event (knob.getDialBounds().getCentre()), wheel);
            expectLessThan (plain (f.processor, ids::kMix), before, "wheel down lowers MIX");
            expectEquals (gestures.begins, 1);
            expectEquals (gestures.ends, 1);
        }

        beginTest ("typed values: units, comma decimals, -inf, garbage refused, out-of-range clamped");
        {
            EditorFixture f;
            auto& clip = f.editor->getClipKnob();
            expect (clip.commitText ("-7.5 dB"));
            expectWithinAbsoluteError (plain (f.processor, ids::kClip), -7.5f, 0.001f);
            expect (clip.commitText ("-4,2"));
            expectWithinAbsoluteError (plain (f.processor, ids::kClip), -4.2f, 0.001f);
            expect (! clip.commitText ("abc"), "text without a number is refused");
            expectWithinAbsoluteError (plain (f.processor, ids::kClip), -4.2f, 0.001f, "and changes nothing");
            expect (clip.commitText ("-inf"));
            expectWithinAbsoluteError (plain (f.processor, ids::kClip), ids::kClipMinDb, 0.001f);
            expect (clip.commitText ("+12"));
            expectWithinAbsoluteError (plain (f.processor, ids::kClip), ids::kClipMaxDb, 0.001f, "clamped to 0 dBFS");

            auto& mix = f.editor->getMixKnob();
            expect (mix.commitText ("35%"));
            expectWithinAbsoluteError (plain (f.processor, ids::kMix), 35.0f, 0.001f);
        }

        beginTest ("a click on the value opens text entry; the knob follows host automation");
        {
            EditorFixture f;
            auto& knob = f.editor->getOutputKnob();
            Mouse m (knob);
            const auto v = knob.getValueBounds().getCentre();
            m.press (v);
            m.release (v, false);
            expect (knob.isTextEntryOpen(), "text entry is open");

            auto* param = f.processor.apvts.getParameter (ids::kOutput);
            param->setValueNotifyingHost (param->convertTo0to1 (6.0f));      // as if the host's automation moved it
            expectWithinAbsoluteError (knob.getNormalised(), param->convertTo0to1 (6.0f), 1.0e-4f);
        }
    }
};
static KnobTests knobTests;

//==================================================================================================
struct ControlTests : public juce::UnitTest
{
    ControlTests() : juce::UnitTest ("Mode, oversampling, LINK and CEILING controls", "Clipline-UI") {}

    void runTest() override
    {
        beginTest ("segmented controls select by click and follow the parameter");
        {
            EditorFixture f;
            auto& mode = f.editor->getModeControl();
            Mouse m (mode);
            for (int i = 0; i < mode.getNumSegments(); ++i)
            {
                m.press (mode.getSegmentBounds (i).getCentre());
                expectEquals ((int) std::lround (plain (f.processor, ids::kMode)), i, "mode segment " + juce::String (i));
                expectEquals (mode.getSelectedIndex(), i);
            }

            auto& os = f.editor->getOversamplingControl();
            Mouse mo (os);
            mo.press (os.getSegmentBounds (3).getCentre());
            expectEquals ((int) std::lround (plain (f.processor, ids::kOversampling)), 3);

            auto* param = f.processor.apvts.getParameter (ids::kOversampling);
            param->setValueNotifyingHost (param->convertTo0to1 (1.0f));
            expectEquals (os.getSelectedIndex(), 1, "follows the host");
        }

        beginTest ("LINK and CEILING chips toggle their parameters");
        {
            EditorFixture f;
            for (auto* chip : { &f.editor->getLinkChip(), &f.editor->getCeilingChip() })
            {
                const char* id = chip == &f.editor->getLinkChip() ? ids::kLink : ids::kCeiling;
                Mouse m (*chip);
                const bool before = plain (f.processor, id) > 0.5f;
                m.press (chip->getLocalBounds().toFloat().getCentre());
                expect ((plain (f.processor, id) > 0.5f) != before, juce::String (id) + " toggled");
                expect (chip->isOn() == (plain (f.processor, id) > 0.5f), juce::String (id) + " chip shows the state");
                m.press (chip->getLocalBounds().toFloat().getCentre());
                expect ((plain (f.processor, id) > 0.5f) == before, juce::String (id) + " toggled back");
            }
        }
    }
};
static ControlTests controlTests;

//==================================================================================================
struct WaveformAndEditorTests : public juce::UnitTest
{
    WaveformAndEditorTests() : juce::UnitTest ("Waveform, meters and window", "Clipline-UI") {}

    void runTest() override
    {
        beginTest ("dragging the threshold line in the waveform sets CLIP");
        {
            EditorFixture f;
            auto& wave = f.editor->getWaveform();
            const float x = (float) wave.getWidth() * 0.5f;

            auto findLine = [&]
            {
                for (float y = 0.0f; y < (float) wave.getHeight() * 0.5f; y += 0.5f)
                    if (wave.isOverThresholdLine ({ x, y }))
                        return y;
                return -1.0f;
            };
            const float lineY = findLine();
            expect (lineY >= 0.0f, "the upper threshold line can be grabbed");

            GestureCounter gestures (*f.processor.apvts.getParameter (ids::kClip));
            Mouse m (wave);
            m.press ({ x, lineY });
            m.dragTo ({ x, lineY + 30.0f });                 // towards the centre: lower ceiling
            m.release ({ x, lineY + 30.0f }, true);
            expectLessThan (plain (f.processor, ids::kClip), ids::kClipDefaultDb - 0.5f);
            expectEquals (gestures.begins, 1);
            expectEquals (gestures.ends, 1);
            expectWithinAbsoluteError (wave.getThresholdDb(), plain (f.processor, ids::kClip), 0.051f, "line follows the parameter");

            wave.mouseDoubleClick (m.event ({ x, (float) wave.getHeight() * 0.5f }, juce::ModifierKeys::leftButtonModifier, 2));
            expect (plain (f.processor, ids::kClip) < ids::kClipDefaultDb - 0.5f, "double-click away from the line does nothing");
            wave.mouseDoubleClick (m.event ({ x, findLine() }, juce::ModifierKeys::leftButtonModifier, 2));
            expectWithinAbsoluteError (plain (f.processor, ids::kClip), ids::kClipDefaultDb, 0.051f, "double-click on the line resets");
        }

        beginTest ("meters and latency read-out follow the real engine");
        {
            EditorFixture f;
            juce::MidiBuffer midi;
            auto loud = makeSine (2, 512, 48000.0, 997.0, 0.95f);
            for (int i = 0; i < 40; ++i)
            {
                auto block = loud;
                f.processor.processBlock (block, midi);
            }
            f.editor->updateFromEngine (0.05);
            const auto snap = f.editor->getMonitor().getSnapshot();
            expectWithinAbsoluteError (snap.inputMaxDb, (float) ampToDb (0.95), 0.2f, "input peak read-out");
            expectWithinAbsoluteError (snap.outputMaxDb, ids::kClipDefaultDb, 0.05f, "output peak read-out = ceiling");
            expect (snap.clipActive, "CLIP indicator lit");
            expectGreaterThan (snap.clipHoldDb, 0.0f, "CLIP shows how far the threshold was exceeded");
            expect (! snap.overLatched, "no overload");
            expect (f.editor->getLatencyText().contains ("60 smp"), "latency read-out: " + f.editor->getLatencyText());
        }

        beginTest ("resizing scales the whole interface with a fixed aspect ratio");
        {
            EditorFixture f;
            for (int w : { 675, 900, 1200, 1440 })
            {
                f.editor->setSize (w, (int) std::lround (w * 560.0 / 900.0));
                const auto t = f.editor->getRoot().getTransform();
                expectWithinAbsoluteError (t.mat00, (float) w / 900.0f, 1.0e-4f, "scale at width " + juce::String (w));
                expectWithinAbsoluteError (t.mat11, (float) w / 900.0f, 1.0e-4f);
            }
            auto* c = f.editor->getConstrainer();
            expect (c != nullptr && std::abs (c->getFixedAspectRatio() - 900.0 / 560.0) < 1.0e-9, "aspect ratio is fixed");
        }
    }
};
static WaveformAndEditorTests waveformAndEditorTests;
