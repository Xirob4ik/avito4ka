#pragma once

// Parameter identifiers and ranges. Header-only and free of JUCE types so the DSP core, the tests
// and the plugin all agree on one definition.
//
// IMPORTANT: parameter IDs are part of the saved-project format and of DAW automation lanes.
// Never rename or reuse an ID once a release has shipped; add new IDs (and bump kVersionHint
// for the new parameter) instead.

namespace clipline::params
{
    // ---- Stable identifiers -------------------------------------------------------------------
    inline constexpr const char* kInput        = "input";
    inline constexpr const char* kClip         = "clip";
    inline constexpr const char* kOutput       = "output";
    inline constexpr const char* kMix          = "mix";
    inline constexpr const char* kMode         = "mode";
    inline constexpr const char* kOversampling = "os";
    inline constexpr const char* kLink         = "link";
    inline constexpr const char* kCeiling      = "ceiling";   // added in 0.1.0 with version hint 1

    // Version hint passed to juce::ParameterID (used by AU/VST3 hosts that care about additions).
    inline constexpr int kVersionHint = 1;

    // ---- Ranges (all in the units shown to the user) --------------------------------------------
    inline constexpr float kGainMinDb       = -24.0f;
    inline constexpr float kGainMaxDb       = 24.0f;
    inline constexpr float kGainStepDb      = 0.1f;
    inline constexpr float kInputDefaultDb  = 0.0f;
    inline constexpr float kOutputDefaultDb = 0.0f;

    inline constexpr float kClipMinDb       = -24.0f;   // dBFS, relative to digital full scale
    inline constexpr float kClipMaxDb       = 0.0f;
    inline constexpr float kClipStepDb      = 0.1f;
    inline constexpr float kClipDefaultDb   = -3.0f;

    inline constexpr float kMixMinPercent     = 0.0f;
    inline constexpr float kMixMaxPercent     = 100.0f;
    inline constexpr float kMixStepPercent    = 0.1f;
    inline constexpr float kMixDefaultPercent = 100.0f;

    inline constexpr int   kModeDefault         = 0;    // HARD
    inline constexpr int   kOversamplingDefault = 2;    // 4x
    inline constexpr bool  kLinkDefault         = true;
    inline constexpr bool  kCeilingDefault      = true;  // CLIP is a hard ceiling (base-rate safety stage)

    inline constexpr int   kNumModes        = 4;
    inline constexpr int   kNumOversampling = 4;
}
