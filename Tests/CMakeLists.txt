# One test executable: DSP unit tests, engine tests, plugin (processor) tests and the UI snapshot renderer.
# It compiles the same sources as the plugin (minus the format entry point), so what is tested is what ships.

juce_add_console_app(ClipperTests PRODUCT_NAME "ClipperTests")

file(GLOB CLIPLINE_TEST_SOURCES CONFIGURE_DEPENDS *.cpp *.h)   # top level only: HostCheck/ is its own target

target_sources(ClipperTests PRIVATE ${CLIPLINE_PLUGIN_SOURCES} ${CLIPLINE_TEST_SOURCES})

target_include_directories(ClipperTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Source
    ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(ClipperTests PRIVATE
    CLIPLINE_VERSION_STRING="${PROJECT_VERSION}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_UNIT_TESTS=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    CLIPLINE_TESTING=1)

target_link_libraries(ClipperTests
    PRIVATE
        ClipperAssets
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

add_test(NAME ClipperTests COMMAND ClipperTests)
set_tests_properties(ClipperTests PROPERTIES TIMEOUT 1800)

# A minimal VST3 host that loads the built plugin through the real VST3 interfaces (see HostCheck/HostCheck.cpp).
#   ClipperHostCheck <build>/Clipline_artefacts/Release/VST3/Clipline.vst3
juce_add_console_app(ClipperHostCheck PRODUCT_NAME "ClipperHostCheck")
target_sources(ClipperHostCheck PRIVATE HostCheck/HostCheck.cpp)
target_compile_definitions(ClipperHostCheck PRIVATE
    JUCE_PLUGINHOST_VST3=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)
target_link_libraries(ClipperHostCheck
    PRIVATE
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags)
