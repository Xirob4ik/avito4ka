#include "Parameters.h"

namespace clipline::params
{
    namespace
    {
        juce::ParameterID makeId (const char* id)
        {
            return juce::ParameterID { id, kVersionHint };
        }
    }

    juce::String formatGainDb (float db)
    {
        const auto rounded = std::round (db * 10.0f) / 10.0f;
        return (rounded > 0.0f ? "+" : "") + juce::String (rounded, 1) + " dB";
    }

    juce::String formatClipDb (float db)
    {
        return juce::String (std::round (db * 10.0f) / 10.0f, 1) + " dB";
    }

    juce::String formatPercent (float percent)
    {
        const auto rounded = std::round (percent * 10.0f) / 10.0f;
        if (std::abs (rounded - std::round (rounded)) < 0.05f)
            return juce::String ((int) std::round (rounded)) + "%";
        return juce::String (rounded, 1) + "%";
    }

    float parseNumber (const juce::String& text, float fallback)
    {
        if (text.containsIgnoreCase ("inf"))
            return fallback;

        const auto cleaned = text.trim()
                                 .replaceCharacter (',', '.')
                                 .retainCharacters ("0123456789.-+");
        if (cleaned.isEmpty() || ! cleaned.containsAnyOf ("0123456789"))
            return fallback;

        return cleaned.getFloatValue();
    }

    const juce::StringArray& modeShortNames()
    {
        static const juce::StringArray names { "HARD", "SOFT", "CUBIC", "FOLD" };
        return names;
    }

    const juce::StringArray& oversamplingNames()
    {
        static const juce::StringArray names { "1x", "2x", "4x", "8x" };
        return names;
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using FloatAttrs  = juce::AudioParameterFloatAttributes;
        using ChoiceAttrs = juce::AudioParameterChoiceAttributes;
        using BoolAttrs   = juce::AudioParameterBoolAttributes;

        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            makeId (kInput), "Input Gain",
            juce::NormalisableRange<float> (kGainMinDb, kGainMaxDb, kGainStepDb), kInputDefaultDb,
            FloatAttrs().withLabel ("dB")
                        .withStringFromValueFunction ([] (float v, int) { return formatGainDb (v); })
                        .withValueFromStringFunction ([] (const juce::String& t) { return parseNumber (t, kGainMinDb); })));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            makeId (kClip), "Clip Threshold",
            juce::NormalisableRange<float> (kClipMinDb, kClipMaxDb, kClipStepDb), kClipDefaultDb,
            FloatAttrs().withLabel ("dBFS")
                        .withStringFromValueFunction ([] (float v, int) { return formatClipDb (v); })
                        .withValueFromStringFunction ([] (const juce::String& t) { return parseNumber (t, kClipMinDb); })));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            makeId (kOutput), "Output Gain",
            juce::NormalisableRange<float> (kGainMinDb, kGainMaxDb, kGainStepDb), kOutputDefaultDb,
            FloatAttrs().withLabel ("dB")
                        .withStringFromValueFunction ([] (float v, int) { return formatGainDb (v); })
                        .withValueFromStringFunction ([] (const juce::String& t) { return parseNumber (t, kGainMinDb); })));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            makeId (kMix), "Dry/Wet Mix",
            juce::NormalisableRange<float> (kMixMinPercent, kMixMaxPercent, kMixStepPercent), kMixDefaultPercent,
            FloatAttrs().withLabel ("%")
                        .withStringFromValueFunction ([] (float v, int) { return formatPercent (v); })
                        .withValueFromStringFunction ([] (const juce::String& t) { return parseNumber (t, kMixMinPercent); })));

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            makeId (kMode), "Clipping Mode",
            juce::StringArray { "Hard", "Soft", "Cubic", "Wavefold" }, kModeDefault,
            ChoiceAttrs()));

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            makeId (kOversampling), "Oversampling",
            oversamplingNames(), kOversamplingDefault,
            ChoiceAttrs()));

        layout.add (std::make_unique<juce::AudioParameterBool> (
            makeId (kLink), "Stereo Link", kLinkDefault,
            BoolAttrs().withStringFromValueFunction ([] (bool v, int) { return v ? juce::String ("On") : juce::String ("Off"); })));

        layout.add (std::make_unique<juce::AudioParameterBool> (
            makeId (kCeiling), "True Ceiling", kCeilingDefault,
            BoolAttrs().withStringFromValueFunction ([] (bool v, int) { return v ? juce::String ("On") : juce::String ("Off"); })));

        return layout;
    }
}
