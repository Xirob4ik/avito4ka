// Test runner.
//
//   ClipperTests                    run every test
//   ClipperTests --category <name>  run one category (Clipline-DSP, Clipline-Engine, Clipline-Plugin, Clipline-UI)
//   ClipperTests -v                 also log passing checks
//   ClipperTests --design-search    offline search for oversampling stage designs with integer latency
//   ClipperTests --snapshot <png>   render the plugin editor to a PNG (see SnapshotTool.cpp)

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <iostream>

#include "DSP/OversamplingProcessor.h"

int runSnapshotTool (const juce::StringArray& args);    // SnapshotTool.cpp

namespace
{
    class ConsoleRunner : public juce::UnitTestRunner
    {
    protected:
        void logMessage (const juce::String& message) override
        {
            std::cout << message << std::endl;
        }
    };

    int runDesignSearch()
    {
        using OP = clipline::dsp::OversamplingProcessor;
        auto designs = OP::stageDesigns();

        std::cout << "Stage design search (latency in 1/8 samples; integer latency <=> multiple of 8)\n";
        for (int stages = 1; stages <= OP::kNumStages; ++stages)
        {
            auto eighths = OP::designLatencyEighths (designs, stages);
            std::cout << "  " << (1 << stages) << "x  start: " << eighths << (eighths % 8 == 0 ? "  (integer)" : "  (fractional)") << "\n";
        }

        for (int stage = 1; stage < OP::kNumStages; ++stage)
        {
            if (OP::designLatencyEighths (designs, stage + 1) % 8 == 0)
                continue;

            bool found = false;
            auto candidate = designs;

            for (int total = 1; total < 80 && ! found; ++total)
                for (int a = 0; a <= total && ! found; ++a)
                {
                    const int b = total - a;
                    candidate = designs;
                    auto& d = candidate[(size_t) stage];
                    d.stopbandUpDb   = designs[(size_t) stage].stopbandUpDb   - 0.5f * (float) a;
                    d.stopbandDownDb = designs[(size_t) stage].stopbandDownDb - 0.5f * (float) b;

                    if (OP::designLatencyEighths (candidate, stage + 1) % 8 == 0)
                    {
                        designs = candidate;
                        found = true;
                        std::cout << "  stage " << stage << ": stopbandUp " << d.stopbandUpDb
                                  << "  stopbandDown " << d.stopbandDownDb
                                  << "  -> " << (1 << (stage + 1)) << "x latency = "
                                  << (double) OP::designLatencyEighths (designs, stage + 1) / 8.0 << " samples\n";
                    }
                }

            if (! found)
            {
                std::cout << "  stage " << stage << ": NO SOLUTION FOUND\n";
                return 1;
            }
        }

        std::cout << "\nTable to paste into OversamplingProcessor::stageDesigns():\n";
        for (const auto& d : designs)
            std::cout << "    { " << d.transitionUp << "f, " << d.stopbandUpDb << "f, "
                      << d.transitionDown << "f, " << d.stopbandDownDb << "f },\n";
        return 0;
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::StringArray args;
    for (int i = 1; i < argc; ++i)
        args.add (argv[i]);

    if (args.contains ("--design-search"))
        return runDesignSearch();

    if (args.contains ("--snapshot"))
        return runSnapshotTool (args);

    ConsoleRunner runner;
    runner.setAssertOnFailure (false);
    runner.setPassesAreLogged (args.contains ("-v"));

    // JUCE_UNIT_TESTS also registers JUCE's own module tests; only run ours unless asked otherwise.
    juce::StringArray categories;
    const int categoryIndex = args.indexOf ("--category");
    if (categoryIndex >= 0 && categoryIndex + 1 < args.size())
        categories.add (args[categoryIndex + 1]);
    else
        categories.addArray ({ "Clipline-DSP", "Clipline-Engine", "Clipline-Plugin", "Clipline-UI" });

    int groups = 0, checks = 0, failures = 0;

    auto collect = [&]
    {
        for (int i = 0; i < runner.getNumResults(); ++i)
            if (auto* r = runner.getResult (i))
            {
                ++groups;
                checks += r->passes + r->failures;
                failures += r->failures;
                std::cout << (r->failures == 0 ? "  [ ok ] " : "  [FAIL] ") << r->unitTestName << " / " << r->subcategoryName
                          << "  (" << r->passes << " passed, " << r->failures << " failed)" << std::endl;
            }
    };

    if (args.contains ("--all-juce-tests"))
    {
        runner.runAllTests();
        collect();
    }
    else
    {
        for (const auto& category : categories)
        {
            runner.runTestsInCategory (category);
            collect();          // runTestsInCategory() clears the previous results
        }
    }

    std::cout << "\n==== " << groups << " test groups, " << checks << " checks, "
              << failures << " failures ====" << std::endl;

    return failures == 0 ? 0 : 1;
}
