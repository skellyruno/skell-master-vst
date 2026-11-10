cmake_minimum_required(VERSION 3.22)
project(SkellyMaster VERSION 1.0.0 LANGUAGES C CXX)

include(FetchContent)
if(DEFINED JUCE_PATH AND EXISTS "${JUCE_PATH}/CMakeLists.txt")
    add_subdirectory("${JUCE_PATH}" JUCE)
else()
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.12
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(SkellyMaster
    COMPANY_NAME "Skelly Audio"
    BUNDLE_ID "audio.skelly.master"
    PLUGIN_MANUFACTURER_CODE SkAu
    PLUGIN_CODE SkM1
    FORMATS VST3 Standalone
    PRODUCT_NAME "Skelly Master"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_generate_juce_header(SkellyMaster)
target_sources(SkellyMaster PRIVATE
    Source/PluginProcessor.cpp Source/PluginProcessor.h
    Source/PluginEditor.cpp Source/PluginEditor.h
    Source/UI/StarSkullMark.h
    Source/DSP/MasteringEngine.h)
target_compile_features(SkellyMaster PRIVATE cxx_std_17)
target_compile_definitions(SkellyMaster PUBLIC JUCE_VST3_CAN_REPLACE_VST2=0)
target_compile_definitions(SkellyMaster PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_REPORT_APP_USAGE=0)
target_link_libraries(SkellyMaster PRIVATE
    juce::juce_audio_utils juce::juce_audio_processors
    juce::juce_dsp juce::juce_gui_extra)

# Portable DSP tests do not require JUCE/Windows or any audio interface.
enable_testing()
add_executable(SkellyDSPTests tests/test_mastering.cpp)
target_compile_features(SkellyDSPTests PRIVATE cxx_std_17)
add_test(NAME SkellyDSPTests COMMAND SkellyDSPTests)

# Real JUCE editor + processor smoke tests on the Windows runner. Renders
# offscreen snapshots at multiple sizes and verifies controls/layout/host DSP.
# Link the plugin's shared-code target rather than recompiling the engine/UI.
if(WIN32)
    juce_add_console_app(SkellyUITests PRODUCT_NAME "Skelly Master UI Tests")
    target_sources(SkellyUITests PRIVATE tests/test_ui.cpp)
    target_compile_features(SkellyUITests PRIVATE cxx_std_17)
    target_compile_definitions(SkellyUITests PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_REPORT_APP_USAGE=0)
    target_link_libraries(SkellyUITests PRIVATE SkellyMaster
        juce::juce_audio_utils juce::juce_audio_processors
        juce::juce_dsp juce::juce_gui_extra)
    juce_generate_juce_header(SkellyUITests)
    add_test(NAME SkellyUITests COMMAND SkellyUITests)
    set_tests_properties(SkellyUITests PROPERTIES TIMEOUT 90)
endif()
