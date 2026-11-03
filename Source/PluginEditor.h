#pragma once

#include "PluginProcessor.h"
#include "UI/CustomLookAndFeel.h"
#include "UI/Fonts.h"
#include "UI/KnobComponent.h"
#include "UI/LevelMeter.h"
#include "UI/PresetBar.h"
#include "UI/SegmentedControl.h"
#include "UI/ToggleChip.h"
#include "UI/WaveformDisplay.h"

/** The plugin window.

    All components are laid out on a fixed 900 x 560 design grid inside `Root`; resizing the window scales
    that grid uniformly (fixed aspect ratio), so proportions, spacing and legibility never change.

    A single 60 Hz timer pulls the lock-free meter and waveform data from the audio engine; nothing here
    runs DSP and nothing here blocks the audio thread. */
class ClipperAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer
{
public:
    explicit ClipperAudioProcessorEditor (ClipperAudioProcessor&);
    ~ClipperAudioProcessorEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;
    void parentHierarchyChanged() override { applyRenderingEngine(); }

    /** One UI refresh step: moves engine meter data through the ballistics into the views.
        Called by the timer; public so tests and the snapshot tool can drive it deterministically. */
    void updateFromEngine (double deltaSeconds);

    void resetPeaks();

    // Access for tests and the snapshot tool.
    clipline::ui::KnobComponent& getInputKnob()  noexcept { return inputKnob; }
    clipline::ui::KnobComponent& getClipKnob()   noexcept { return clipKnob; }
    clipline::ui::KnobComponent& getMixKnob()    noexcept { return mixKnob; }
    clipline::ui::KnobComponent& getOutputKnob() noexcept { return outputKnob; }
    clipline::ui::SegmentedControl& getModeControl() noexcept { return modeControl; }
    clipline::ui::SegmentedControl& getOversamplingControl() noexcept { return osControl; }
    clipline::ui::ToggleChip& getLinkChip() noexcept { return linkChip; }
    clipline::ui::ToggleChip& getCeilingChip() noexcept { return ceilingChip; }
    clipline::ui::WaveformDisplay& getWaveform() noexcept { return waveform; }
    clipline::ui::MonitorPanel& getMonitor() noexcept { return monitor; }
    clipline::ui::PresetBar& getPresetBar() noexcept { return presetBar; }
    juce::Component& getRoot() noexcept { return root; }
    juce::String getLatencyText() const { return latencyText; }

private:
    class Root : public juce::Component
    {
    public:
        explicit Root (ClipperAudioProcessorEditor& o) : owner (o) {}
        void paint (juce::Graphics&) override;
    private:
        ClipperAudioProcessorEditor& owner;
    };

    class MenuButton : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        std::function<void()> onClick;
        void paint (juce::Graphics&) override;
        void mouseEnter (const juce::MouseEvent&) override { hot = true; repaint(); }
        void mouseExit (const juce::MouseEvent&) override { hot = false; repaint(); }
        void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
    private:
        bool hot = false;
    };

    void timerCallback() override;
    void applyRenderingEngine();
    void showSettingsMenu();
    void applyTooltipSetting();
    void updateLatencyText();

    ClipperAudioProcessor& audioProcessor;

    // Declaration order matters: the font library and look-and-feel must outlive every child component.
    juce::SharedResourcePointer<clipline::ui::fonts::FontLibrary> fontLibrary;
    clipline::ui::CustomLookAndFeel lookAndFeel;

    Root root;
    clipline::ui::WaveformDisplay waveform;
    clipline::ui::MonitorPanel monitor;
    clipline::ui::LevelMeter topIn, topOut;
    clipline::ui::PresetBar presetBar;
    MenuButton menuButton;

    clipline::ui::KnobComponent inputKnob, clipKnob, mixKnob, outputKnob;
    clipline::ui::SegmentedControl modeControl, osControl;
    clipline::ui::ToggleChip linkChip;
    clipline::ui::ToggleChip ceilingChip;

    std::unique_ptr<juce::TooltipWindow> tooltipWindow;
    juce::ComponentBoundsConstrainer constrainer;

    clipline::dsp::MeterBallistics inBallistics[2], outBallistics[2], clipBallistics;
    bool overLatched = false;
    float clipActiveRemaining = 0.0f;
    juce::String latencyText;
    juce::int64 lastTickMs = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipperAudioProcessorEditor)
};
