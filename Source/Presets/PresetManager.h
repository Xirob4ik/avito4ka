#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

#include "FactoryPresets.h"

namespace clipline
{
    /** Factory and user presets.

        A preset is a plain set of the seven parameter values. Applying one sets every parameter through
        the host-aware parameter API (as complete gestures), so presets behave like the user turning the
        controls: they are undoable in hosts that record that, and they show up in automation recording.

        The name of the last applied preset is kept as a property of the plugin state (saved with the DAW
        project). It is shown with a "modified" marker as soon as any parameter differs from that preset.

        User presets are small XML files (*.clpreset) in <user app data>/Clipline/Presets. */
    class PresetManager
    {
    public:
        struct Values
        {
            float inputDb = 0.0f, clipDb = -3.0f, outputDb = 0.0f, mixPercent = 100.0f;
            int mode = 0, oversampling = 2;
            bool link = true;
            bool ceiling = true;
        };

        struct Entry
        {
            juce::String name, category;
            bool isUser = false;
            juce::File file;
            Values values;
        };

        explicit PresetManager (juce::AudioProcessorValueTreeState&);

        /** Rescans the user preset folder. */
        void refresh();

        const std::vector<Entry>& getEntries() const noexcept { return entries; }
        int findByName (const juce::String&) const noexcept;

        /** Applies entry `index`. Returns false for an invalid index. */
        bool apply (int index);
        /** Steps through the list (wraps around). */
        void step (int delta);

        /** Writes the current parameter values as a user preset (name is sanitised) and selects it. */
        bool saveUserPreset (const juce::String& name);

        juce::String getCurrentName() const;
        int getCurrentIndex() const noexcept { return findByName (getCurrentName()); }
        /** True when the parameters differ from the preset named in the state (or it no longer exists). */
        bool isModified() const;

        Values readCurrentValues() const;
        static juce::File getUserPresetDirectory();
        static Values fromFactory (const presets::PresetValues&) noexcept;

        // Exposed for tests.
        static bool writePresetFile (const juce::File&, const juce::String& name, const Values&);
        static bool readPresetFile (const juce::File&, juce::String& name, Values&);

    private:
        void applyValues (const Values&);

        juce::AudioProcessorValueTreeState& apvts;
        std::vector<Entry> entries;
    };
}
