// Plugin-format entry point. Kept out of the shared source list so that test executables can link
// the processor without defining createPluginFilter().
#include "PluginProcessor.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ClipperAudioProcessor();
}
