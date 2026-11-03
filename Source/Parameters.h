#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "ParameterIds.h"

namespace clipline::params
{
    /** Builds the full, host-visible parameter layout (all parameters are automatable). */
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // Text conversion shared by the host-visible parameter strings and the editor's value fields.
    juce::String formatGainDb  (float db);        // "+3.0 dB"
    juce::String formatClipDb  (float db);        // "-3.0 dB"
    juce::String formatPercent (float percent);   // "100%" / "37.5%"

    /** Lenient numeric parsing: accepts "-3", "-3.5 dB", "3,5", "-inf" (-> fallback = range minimum). */
    float parseNumber (const juce::String& text, float fallback);

    /** Short, upper-case labels used by the editor ("HARD", "SOFT", "CUBIC", "FOLD"). */
    const juce::StringArray& modeShortNames();
    /** Labels for oversampling choices ("1x", "2x", "4x", "8x"). */
    const juce::StringArray& oversamplingNames();
}
