#include "PresetManager.h"

#include "../Branding.h"
#include "../ParameterIds.h"

namespace clipline
{
    namespace ids = clipline::params;

    namespace
    {
        constexpr const char* kPresetNameProperty = "presetName";

        void setParameter (juce::AudioProcessorValueTreeState& apvts, const char* id, float plainValue)
        {
            if (auto* p = apvts.getParameter (id))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
                p->endChangeGesture();
            }
        }

        float getParameter (juce::AudioProcessorValueTreeState& apvts, const char* id, float fallback)
        {
            if (auto* p = apvts.getRawParameterValue (id))
                return p->load();
            return fallback;
        }

        bool close (float a, float b, float tolerance) { return std::abs (a - b) <= tolerance; }
    }

    PresetManager::PresetManager (juce::AudioProcessorValueTreeState& state) : apvts (state)
    {
        refresh();
    }

    PresetManager::Values PresetManager::fromFactory (const presets::PresetValues& f) noexcept
    {
        Values v;
        v.inputDb = f.inputDb;  v.clipDb = f.clipDb;  v.outputDb = f.outputDb;  v.mixPercent = f.mixPercent;
        v.mode = f.mode;  v.oversampling = f.oversampling;  v.link = f.link;  v.ceiling = f.ceiling;
        return v;
    }

    juce::File PresetManager::getUserPresetDirectory()
    {
        // Windows: %APPDATA%\Clipline\Presets.  macOS: ~/Library/Application Support/Clipline/Presets
        // (JUCE's userApplicationDataDirectory is ~/Library on macOS, so the subfolder is added explicitly).
        auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
       #if JUCE_MAC
        base = base.getChildFile ("Application Support");
       #endif
        return base.getChildFile (branding::kUserDataFolder).getChildFile ("Presets");
    }

    void PresetManager::refresh()
    {
        entries.clear();

        for (std::size_t i = 0; i < presets::getNumFactoryPresets(); ++i)
        {
            const auto& f = presets::getFactoryPreset (i);
            Entry e;
            e.name = f.name;
            e.category = f.category;
            e.values = fromFactory (f);
            entries.push_back (std::move (e));
        }

        const auto dir = getUserPresetDirectory();
        if (dir.isDirectory())
        {
            auto files = dir.findChildFiles (juce::File::findFiles, false, juce::String ("*.") + branding::kPresetExtension);
            files.sort();

            for (const auto& file : files)
            {
                juce::String name;
                Values v;
                if (readPresetFile (file, name, v) && name.isNotEmpty() && findByName (name) < 0)
                {
                    Entry e;
                    e.name = name;
                    e.category = "User";
                    e.isUser = true;
                    e.file = file;
                    e.values = v;
                    entries.push_back (std::move (e));
                }
            }
        }
    }

    int PresetManager::findByName (const juce::String& name) const noexcept
    {
        for (size_t i = 0; i < entries.size(); ++i)
            if (entries[i].name == name)
                return (int) i;
        return -1;
    }

    //==============================================================================================
    PresetManager::Values PresetManager::readCurrentValues() const
    {
        Values v;
        v.inputDb    = getParameter (apvts, ids::kInput,  ids::kInputDefaultDb);
        v.clipDb     = getParameter (apvts, ids::kClip,   ids::kClipDefaultDb);
        v.outputDb   = getParameter (apvts, ids::kOutput, ids::kOutputDefaultDb);
        v.mixPercent = getParameter (apvts, ids::kMix,    ids::kMixDefaultPercent);
        v.mode         = juce::jlimit (0, ids::kNumModes - 1, (int) std::lround (getParameter (apvts, ids::kMode, 0.0f)));
        v.oversampling = juce::jlimit (0, ids::kNumOversampling - 1, (int) std::lround (getParameter (apvts, ids::kOversampling, 2.0f)));
        v.link = getParameter (apvts, ids::kLink, 1.0f) > 0.5f;
        v.ceiling = getParameter (apvts, ids::kCeiling, 1.0f) > 0.5f;
        return v;
    }

    void PresetManager::applyValues (const Values& v)
    {
        setParameter (apvts, ids::kInput,  v.inputDb);
        setParameter (apvts, ids::kClip,   v.clipDb);
        setParameter (apvts, ids::kOutput, v.outputDb);
        setParameter (apvts, ids::kMix,    v.mixPercent);
        setParameter (apvts, ids::kMode,   (float) v.mode);
        setParameter (apvts, ids::kOversampling, (float) v.oversampling);
        setParameter (apvts, ids::kLink,   v.link ? 1.0f : 0.0f);
        setParameter (apvts, ids::kCeiling, v.ceiling ? 1.0f : 0.0f);
    }

    bool PresetManager::apply (int index)
    {
        if (index < 0 || index >= (int) entries.size())
            return false;

        const auto& e = entries[(size_t) index];
        applyValues (e.values);
        apvts.state.setProperty (kPresetNameProperty, e.name, nullptr);
        return true;
    }

    void PresetManager::step (int delta)
    {
        if (entries.empty())
            return;

        const int count = (int) entries.size();
        int current = getCurrentIndex();
        if (current < 0)
            current = delta > 0 ? -1 : 0;

        apply (((current + delta) % count + count) % count);
    }

    juce::String PresetManager::getCurrentName() const
    {
        const auto name = apvts.state.getProperty (kPresetNameProperty).toString();
        return name.isNotEmpty() ? name : juce::String ("Init");
    }

    bool PresetManager::isModified() const
    {
        const int index = getCurrentIndex();
        if (index < 0)
            return true;

        const auto& p = entries[(size_t) index].values;
        const auto c = readCurrentValues();
        return ! (close (p.inputDb, c.inputDb, 0.05f) && close (p.clipDb, c.clipDb, 0.05f)
                  && close (p.outputDb, c.outputDb, 0.05f) && close (p.mixPercent, c.mixPercent, 0.05f)
                  && p.mode == c.mode && p.oversampling == c.oversampling && p.link == c.link && p.ceiling == c.ceiling);
    }

    //==============================================================================================
    bool PresetManager::saveUserPreset (const juce::String& rawName)
    {
        const auto name = rawName.trim().substring (0, 48);
        if (name.isEmpty())
            return false;

        // Factory names are reserved so a user file can never shadow them.
        for (std::size_t i = 0; i < presets::getNumFactoryPresets(); ++i)
            if (name == presets::getFactoryPreset (i).name)
                return false;

        const auto dir = getUserPresetDirectory();
        if (! dir.createDirectory().wasOk())
            return false;

        const auto file = dir.getChildFile (juce::File::createLegalFileName (name)).withFileExtension (branding::kPresetExtension);
        if (! writePresetFile (file, name, readCurrentValues()))
            return false;

        refresh();
        apvts.state.setProperty (kPresetNameProperty, name, nullptr);
        return true;
    }

    bool PresetManager::writePresetFile (const juce::File& file, const juce::String& name, const Values& v)
    {
        juce::XmlElement xml ("ClipperPreset");
        xml.setAttribute ("version", 1);
        xml.setAttribute ("name", name);
        xml.setAttribute (ids::kInput,  (double) v.inputDb);
        xml.setAttribute (ids::kClip,   (double) v.clipDb);
        xml.setAttribute (ids::kOutput, (double) v.outputDb);
        xml.setAttribute (ids::kMix,    (double) v.mixPercent);
        xml.setAttribute (ids::kMode,   v.mode);
        xml.setAttribute (ids::kOversampling, v.oversampling);
        xml.setAttribute (ids::kLink,   v.link ? 1 : 0);
        xml.setAttribute (ids::kCeiling, v.ceiling ? 1 : 0);
        return xml.writeTo (file);
    }

    bool PresetManager::readPresetFile (const juce::File& file, juce::String& name, Values& v)
    {
        const auto xml = juce::parseXML (file);
        if (xml == nullptr || ! xml->hasTagName ("ClipperPreset"))
            return false;

        name = xml->getStringAttribute ("name");

        // Values come from a file the user may have edited: clamp everything into the legal ranges.
        v.inputDb    = juce::jlimit (ids::kGainMinDb, ids::kGainMaxDb, (float) xml->getDoubleAttribute (ids::kInput,  ids::kInputDefaultDb));
        v.clipDb     = juce::jlimit (ids::kClipMinDb, ids::kClipMaxDb, (float) xml->getDoubleAttribute (ids::kClip,   ids::kClipDefaultDb));
        v.outputDb   = juce::jlimit (ids::kGainMinDb, ids::kGainMaxDb, (float) xml->getDoubleAttribute (ids::kOutput, ids::kOutputDefaultDb));
        v.mixPercent = juce::jlimit (ids::kMixMinPercent, ids::kMixMaxPercent, (float) xml->getDoubleAttribute (ids::kMix, ids::kMixDefaultPercent));
        v.mode         = juce::jlimit (0, ids::kNumModes - 1, xml->getIntAttribute (ids::kMode, ids::kModeDefault));
        v.oversampling = juce::jlimit (0, ids::kNumOversampling - 1, xml->getIntAttribute (ids::kOversampling, ids::kOversamplingDefault));
        v.link = xml->getIntAttribute (ids::kLink, ids::kLinkDefault ? 1 : 0) != 0;
        v.ceiling = xml->getIntAttribute (ids::kCeiling, ids::kCeilingDefault ? 1 : 0) != 0;   // older files: on
        return true;
    }
}
