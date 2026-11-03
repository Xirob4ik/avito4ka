#include "PluginEditor.h"

#include "Branding.h"
#include "UI/Logo.h"
#include "UI/Theme.h"
#include "UI/UiSettings.h"

namespace ui = clipline::ui;
namespace ids = clipline::params;
using namespace ui::theme;

namespace
{
    // ---- design grid (logical pixels) -----------------------------------------------------------------
    constexpr int kMargin = 20;
    constexpr int kTopBarH = 52;
    const juce::Rectangle<int> kWaveform { kMargin, 64, 744, 292 };
    const juce::Rectangle<int> kMonitor  { 776, 64, 104, 292 };
    const juce::Rectangle<int> kControlsPanel { kMargin, 368, 860, 172 };

    // All four knobs share one height so titles and values line up; the dial itself is centred between.
    const juce::Rectangle<int> kInputKnob  { 36, 376, 110, 156 };
    const juce::Rectangle<int> kClipKnob   { 156, 376, 150, 156 };
    const juce::Rectangle<int> kMixKnob    { 316, 376, 110, 156 };
    const juce::Rectangle<int> kOutputKnob { 436, 376, 110, 156 };

    constexpr int kSeparatorX = 566;
    constexpr int kRightX = 586, kRightW = 278;
    const juce::Rectangle<int> kModeLabel { kRightX, 384, 120, 12 };
    const juce::Rectangle<int> kModeSeg   { kRightX, 400, kRightW, 28 };
    const juce::Rectangle<int> kOsLabel   { kRightX, 440, 160, 12 };
    const juce::Rectangle<int> kOsSeg     { kRightX, 456, kRightW, 28 };
    const juce::Rectangle<int> kLinkChip    { kRightX, 496, 84, 28 };
    const juce::Rectangle<int> kCeilingChip { kRightX + 92, 496, 100, 28 };
    const juce::Rectangle<int> kLatency     { kRightX + 100, 438, kRightW - 100, 16 };   // on the OVERSAMPLING label line

    juce::RangedAudioParameter& param (juce::AudioProcessorValueTreeState& apvts, const char* id)
    {
        auto* p = apvts.getParameter (id);
        jassert (p != nullptr);
        return *p;
    }

}

//==================================================================================================
void ClipperAudioProcessorEditor::Root::paint (juce::Graphics& g)
{
    g.fillAll (bg);

    // ---- top bar -----------------------------------------------------------------------------------------
    ui::logo::drawGlyph (g, { 22.0f, 15.0f, 24.0f, 22.0f }, accent);
    ui::logo::drawWordmark (g, { 56.0f, 12.0f, 130.0f, 28.0f });
    g.setColour (hairlineSoft);
    g.fillRect (0, kTopBarH - 1, getWidth(), 1);

    // ---- controls panel ---------------------------------------------------------------------------------------
    g.setColour (surface);
    g.fillRoundedRectangle (kControlsPanel.toFloat(), 8.0f);

    g.setColour (hairlineSoft);
    g.fillRect (kSeparatorX, kControlsPanel.getY() + 20, 1, kControlsPanel.getHeight() - 40);

    g.setColour (textDim);
    g.setFont (ui::fonts::sans (10.0f, ui::fonts::Weight::Medium, 0.16f));
    g.drawText ("MODE", kModeLabel, juce::Justification::centredLeft, false);
    g.drawText ("OVERSAMPLING", kOsLabel, juce::Justification::centredLeft, false);

    g.setColour (owner.audioProcessor.getPublishedLatencySamples() > 0 ? textDim : textFaint);
    g.setFont (ui::fonts::mono (10.5f));
    g.drawText (owner.latencyText, kLatency, juce::Justification::centredRight, false);
}

void ClipperAudioProcessorEditor::MenuButton::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    if (hot)
    {
        g.setColour (surface);
        g.fillRoundedRectangle (b, 6.0f);
    }

    g.setColour (hot ? text : textDim);
    for (int i = -1; i <= 1; ++i)
        g.fillEllipse (b.getCentreX() + (float) i * 6.0f - 1.75f, b.getCentreY() - 1.75f, 3.5f, 3.5f);
}

//==================================================================================================
ClipperAudioProcessorEditor::ClipperAudioProcessorEditor (ClipperAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      root (*this),
      waveform (p.getEngine().getWaveformTap(), param (p.apvts, ids::kClip)),
      topIn  (ui::LevelMeter::Orientation::Horizontal, ui::MonitorPanel::kMinDb, ui::MonitorPanel::kMaxDb),
      topOut (ui::LevelMeter::Orientation::Horizontal, ui::MonitorPanel::kMinDb, ui::MonitorPanel::kMaxDb),
      presetBar (p.presetManager),
      inputKnob  (param (p.apvts, ids::kInput),  "INPUT",  { false, true,  false },
                  "Input gain, applied before the clipper. Push the signal harder into the clipping stage."),
      clipKnob   (param (p.apvts, ids::kClip),   "CLIP",   { true,  false, true },
                  "Clip threshold in dBFS. The ring fills as the ceiling drops: lower values cut more of the signal."),
      mixKnob    (param (p.apvts, ids::kMix),    "MIX",    { false, false, false },
                  "Dry / wet balance. 0% is the original signal, 100% is fully clipped. The dry path is delay-compensated."),
      outputKnob (param (p.apvts, ids::kOutput), "OUTPUT", { false, true,  false },
                  "Output gain, applied after clipping and mixing. It can raise the level above the CLIP threshold."),
      modeControl (param (p.apvts, ids::kMode), { "HARD", "SOFT", "CUBIC", "FOLD" },
                   { "HARD: clamps the signal at the threshold. The loudest, most aggressive character.",
                     "SOFT: tanh saturation. Rounds the peaks smoothly and approaches the threshold gradually.",
                     "CUBIC: polynomial saturation that reaches the threshold at 1.5x and stays flat beyond.",
                     "FOLD: wavefolder. Instead of flattening, peaks beyond the threshold are reflected back (experimental)." }),
      osControl (param (p.apvts, ids::kOversampling), { "1x", "2x", "4x", "8x" },
                 { "No oversampling: lowest CPU, most aliasing, no latency.",
                   "2x oversampling: clipping runs at twice the sample rate.",
                   "4x oversampling: clipping runs at four times the sample rate.",
                   "8x oversampling: cleanest, highest CPU and latency." }),
      linkChip (param (p.apvts, ids::kLink), "LINK",
                "Stereo link. On: one gain from the louder channel is applied to both, which preserves the stereo image. "
                "Off: each channel is clipped independently."),
      ceilingChip (param (p.apvts, ids::kCeiling), "CEILING",
                   "True ceiling. On: the output never exceeds CLIP, even after the oversampling filters (which can ring "
                   "up to ~4 dB above it on dense material). Off: slightly cleaner oversampling on heavily clipped highs, "
                   "but peaks may pass the threshold.")
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (root);
    root.setBounds (0, 0, designWidth, designHeight);

    for (auto* c : std::initializer_list<juce::Component*> { &waveform, &monitor, &topIn, &topOut, &presetBar, &menuButton,
                                                              &inputKnob, &clipKnob, &mixKnob, &outputKnob,
                                                              &modeControl, &osControl, &linkChip, &ceilingChip })
        root.addAndMakeVisible (*c);

    waveform.setBounds (kWaveform);
    monitor.setBounds (kMonitor);
    presetBar.setBounds (200, 12, 268, 28);
    topIn.setBounds (494, 14, 160, 24);
    topOut.setBounds (672, 14, 160, 24);
    topIn.setCaption ("IN");
    topOut.setCaption ("OUT");
    menuButton.setBounds (848, 12, 32, 28);
    menuButton.setTooltip ("Settings");
    menuButton.onClick = [this] { showSettingsMenu(); };

    inputKnob.setBounds (kInputKnob);
    clipKnob.setBounds (kClipKnob);
    mixKnob.setBounds (kMixKnob);
    outputKnob.setBounds (kOutputKnob);
    modeControl.setBounds (kModeSeg);
    osControl.setBounds (kOsSeg);
    linkChip.setBounds (kLinkChip);
    ceilingChip.setBounds (kCeilingChip);

    waveform.setWindowMs (ui::settings::getWindowMs());
    waveform.onWindowMsChanged = [] (float ms) { ui::settings::setWindowMs (ms); };
    monitor.onResetRequested = [this] { resetPeaks(); };

    for (auto& b : inBallistics)  b.reset();
    for (auto& b : outBallistics) b.reset();
    clipBallistics.setTimes (12.0f, 1.2f);
    clipBallistics.reset();
    updateLatencyText();

    constrainer.setFixedAspectRatio ((double) designWidth / (double) designHeight);
    constrainer.setSizeLimits (designWidth * 3 / 4, designHeight * 3 / 4, designWidth * 8 / 5, designHeight * 8 / 5);
    setConstrainer (&constrainer);
    setResizable (true, true);

    const int width = ui::settings::getEditorWidth();
    setSize (width, (int) std::lround ((double) width * designHeight / designWidth));

    applyTooltipSetting();
    lastTickMs = juce::Time::getMillisecondCounter();
    startTimerHz (60);
}

ClipperAudioProcessorEditor::~ClipperAudioProcessorEditor()
{
    stopTimer();
    setConstrainer (nullptr);
    tooltipWindow.reset();
    setLookAndFeel (nullptr);
}

void ClipperAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) designWidth;
    root.setTransform (juce::AffineTransform::scale (scale));
    ui::settings::setEditorWidth (getWidth());
}

void ClipperAudioProcessorEditor::applyTooltipSetting()
{
    if (ui::settings::getTooltipsEnabled())
    {
        if (tooltipWindow == nullptr)
            tooltipWindow = std::make_unique<juce::TooltipWindow> (this, 650);
    }
    else
    {
        tooltipWindow.reset();
    }
}

//==================================================================================================
void ClipperAudioProcessorEditor::updateLatencyText()
{
    const int samples = audioProcessor.getPublishedLatencySamples();
    const double fs = audioProcessor.getSampleRate() > 0.0 ? audioProcessor.getSampleRate() : 44100.0;
    const auto newText = "LATENCY " + juce::String (samples) + " smp / " + juce::String (1000.0 * (double) samples / fs, 1) + " ms";
    if (newText != latencyText)
    {
        latencyText = newText;
        root.repaint (kLatency);
    }
}

void ClipperAudioProcessorEditor::resetPeaks()
{
    for (auto& b : inBallistics)  b.resetHold();
    for (auto& b : outBallistics) b.resetHold();
    clipBallistics.resetHold();
    overLatched = false;
}

void ClipperAudioProcessorEditor::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounter();
    const double dt = juce::jlimit (0.001, 0.25, (double) (now - lastTickMs) * 0.001);
    lastTickMs = now;
    updateFromEngine (dt);
}

void ClipperAudioProcessorEditor::updateFromEngine (double dtSeconds)
{
    const float dt = (float) dtSeconds;
    auto& meters = audioProcessor.getEngine().getMeters();
    const bool mono = audioProcessor.getMainBusNumInputChannels() < 2;

    float inPeak[2], outPeak[2];
    for (int ch = 0; ch < 2; ++ch)
    {
        inPeak[ch]  = meters.takeInputPeak (ch);
        outPeak[ch] = meters.takeOutputPeak (ch);
    }
    if (mono) { inPeak[1] = inPeak[0]; outPeak[1] = outPeak[0]; }

    for (int ch = 0; ch < 2; ++ch)
    {
        inBallistics[ch].update (inPeak[ch], dt);
        outBallistics[ch].update (outPeak[ch], dt);
    }

    if (std::max (outPeak[0], outPeak[1]) >= 1.0f)
        overLatched = true;

    const float clipRatio = meters.takeClipRatio();
    clipBallistics.update (clipRatio, dt);
    if (clipRatio > 1.0f)
        clipActiveRemaining = 0.15f;
    else
        clipActiveRemaining = std::max (0.0f, clipActiveRemaining - dt);

    ui::MonitorPanel::Snapshot s;
    for (int ch = 0; ch < 2; ++ch)
    {
        s.input.levelDb[ch]  = inBallistics[ch].getLevelDb();
        s.input.holdDb[ch]   = inBallistics[ch].getHoldDb();
        s.output.levelDb[ch] = outBallistics[ch].getLevelDb();
        s.output.holdDb[ch]  = outBallistics[ch].getHoldDb();
    }
    s.inputMaxDb  = std::max (inBallistics[0].getMaxDb(),  inBallistics[1].getMaxDb());
    s.outputMaxDb = std::max (outBallistics[0].getMaxDb(), outBallistics[1].getMaxDb());
    s.clipActive = clipActiveRemaining > 0.0f;
    s.clipHoldDb = clipBallistics.getHoldDb();
    s.overLatched = overLatched;
    monitor.update (s);

    topIn.setValues (s.input);
    topOut.setValues (s.output);
    topIn.setReadout (ui::formatLevelDb (std::max (s.input.holdDb[0], s.input.holdDb[1])));
    topOut.setReadout (ui::formatLevelDb (std::max (s.output.holdDb[0], s.output.holdDb[1])));

    waveform.refresh();
    presetBar.refresh();
    updateLatencyText();
}

//==================================================================================================
void ClipperAudioProcessorEditor::applyRenderingEngine()
{
    // Windows offers "Software Renderer" and "Direct2D" (the JUCE 8 default); other platforms have one engine.
    if (auto* peer = getPeer())
    {
        const auto engines = peer->getAvailableRenderingEngines();
        const int wanted = engines.indexOf (ui::settings::getSoftwareRenderer() ? "Software Renderer" : "Direct2D");
        if (wanted >= 0 && wanted != peer->getCurrentRenderingEngine())
            peer->setCurrentRenderingEngine (wanted);
    }
}

void ClipperAudioProcessorEditor::showSettingsMenu()
{
    juce::PopupMenu windowMenu;
    for (int i = 0; i < 5; ++i)
    {
        const float ms = ui::WaveformDisplay::kWindowChoicesMs[i];
        windowMenu.addItem (100 + i, juce::String ((int) ms) + " ms", true, std::abs (waveform.getWindowMs() - ms) < 0.01f);
    }

    juce::PopupMenu menu;
    menu.addSectionHeader ("Display");
    menu.addSubMenu ("Waveform time window", windowMenu);
    menu.addItem (1, "Tooltips", true, ui::settings::getTooltipsEnabled());
    menu.addItem (2, "Reset peak values");
   #if JUCE_WINDOWS
    menu.addItem (5, "Software rendering (if graphics glitch)", true, ui::settings::getSoftwareRenderer());
   #endif
    menu.addSeparator();
    menu.addItem (3, "Show preset folder");
    menu.addItem (4, "About");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&menuButton).withMinimumWidth (200),
                        [safe = juce::Component::SafePointer<ClipperAudioProcessorEditor> (this)] (int result)
                        {
                            if (safe == nullptr || result == 0)
                                return;

                            if (result >= 100)
                            {
                                safe->waveform.setWindowMs (ui::WaveformDisplay::kWindowChoicesMs[result - 100]);
                            }
                            else if (result == 1)
                            {
                                ui::settings::setTooltipsEnabled (! ui::settings::getTooltipsEnabled());
                                safe->applyTooltipSetting();
                            }
                            else if (result == 2)
                            {
                                safe->resetPeaks();
                            }
                            else if (result == 5)
                            {
                                ui::settings::setSoftwareRenderer (! ui::settings::getSoftwareRenderer());
                                safe->applyRenderingEngine();
                            }
                            else if (result == 3)
                            {
                                const auto dir = clipline::PresetManager::getUserPresetDirectory();
                                dir.createDirectory();
                                dir.revealToUser();
                            }
                            else if (result == 4)
                            {
                                juce::AlertWindow::showMessageBoxAsync (
                                    juce::MessageBoxIconType::NoIcon, juce::String ("Clipline ") + clipline::branding::kVersion,
                                    "A clipper with oversampling, four clipping algorithms and honest meters.\n\n"
                                    "Built with JUCE (AGPLv3 / commercial licence).\n"
                                    "Typefaces: Geist and Geist Mono, SIL Open Font License 1.1.");
                            }
                        });
}
