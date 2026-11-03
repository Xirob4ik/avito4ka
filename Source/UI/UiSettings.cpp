#include "UiSettings.h"

#include <juce_data_structures/juce_data_structures.h>

#include "../Branding.h"
#include "Theme.h"

namespace clipline::ui::settings
{
    namespace
    {
        juce::PropertiesFile& file()
        {
            // Saved immediately on every change (these change rarely), so no background timer is needed
            // and nothing has to be flushed when the plugin unloads.
            static juce::PropertiesFile instance = []
            {
                juce::PropertiesFile::Options options;
                options.applicationName = "ui";
                options.filenameSuffix = ".settings";
                options.folderName = branding::kUserDataFolder;
                options.osxLibrarySubFolder = "Application Support";
                options.storageFormat = juce::PropertiesFile::storeAsXML;
                options.millisecondsBeforeSaving = 0;
                return juce::PropertiesFile (options.getDefaultFile(), options);
            }();
            return instance;
        }
    }

    float getWindowMs()                    { return juce::jlimit (5.0f, 80.0f, (float) file().getDoubleValue ("waveformWindowMs", 20.0)); }
    void  setWindowMs (float ms)           { file().setValue ("waveformWindowMs", (double) ms); }

    bool  getTooltipsEnabled()             { return file().getBoolValue ("tooltips", true); }
    void  setTooltipsEnabled (bool on)     { file().setValue ("tooltips", on); }

    bool  getSoftwareRenderer()            { return file().getBoolValue ("softwareRenderer", false); }
    void  setSoftwareRenderer (bool on)    { file().setValue ("softwareRenderer", on); }

    int getEditorWidth()
    {
        return juce::jlimit (theme::designWidth * 3 / 4, theme::designWidth * 8 / 5, file().getIntValue ("editorWidth", theme::designWidth));
    }

    void setEditorWidth (int width)        { file().setValue ("editorWidth", width); }
}
