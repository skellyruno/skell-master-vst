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

# Image-derived hardware component assets are stored separately and compiled
# into the plugin once. No file I/O, disk dependencies or image decoding occur
# in the audio processing thread.
juce_add_binary_data(SkellyMasterPhotoAssets
    SOURCES Source/UI/Assets/knob_photo.png)

juce_generate_juce_header(SkellyMaster)
target_sources(SkellyMaster PRIVATE
    Source/PluginProcessor.cpp Source/PluginProcessor.h
    Source/PluginEditor.cpp Source/PluginEditor.h
    Source/UI/StarSkullMark.h
    Source/UI/HardwareFinish.h
    Source/DSP/MasteringEngine.h)
target_compile_features(SkellyMaster PRIVATE cxx_std_17)
target_compile_definitions(SkellyMaster PUBLIC JUCE_VST3_CAN_REPLACE_VST2=0)
target_compile_definitions(SkellyMaster PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_REPORT_APP_USAGE=0)
target_link_libraries(SkellyMaster PRIVATE
    SkellyMasterPhotoAssets
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
        SkellyMasterPhotoAssets
        juce::juce_audio_utils juce::juce_audio_processors
        juce::juce_dsp juce::juce_gui_extra)
    juce_generate_juce_header(SkellyUITests)
    add_test(NAME SkellyUITests COMMAND SkellyUITests)
    set_tests_properties(SkellyUITests PROPERTIES TIMEOUT 90)
endif()


# Permanent optional SkellCore headless renderer. The executable links the
# actual plugin shared-code target and is never included in the shipped VST3.
option(SKELLY_MASTER_BUILD_UI_RENDERER "Build real editor with pinned SkellCore renderer" OFF)
if(SKELLY_MASTER_BUILD_UI_RENDERER)
    FetchContent_Declare(SkellCoreRendererSource
        GIT_REPOSITORY https://github.com/skellyruno/skell-vst-template.git
        GIT_TAG 5ac8e885f1563f804c66072797b7038bc2939fae
        SOURCE_SUBDIR SkellCore
        GIT_SHALLOW FALSE)
    set(SKELLCORE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SKELLCORE_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SkellCoreRendererSource)

    # A plain executable avoids conflicting JUCE_STANDALONE_APPLICATION
    # compile definitions inherited from plugin shared code.
    add_executable(SkellyMasterUIRenderer tests/ui_renderer.cpp)
    target_include_directories(SkellyMasterUIRenderer PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}/SkellyMaster_artefacts/JuceLibraryCode")
    target_compile_features(SkellyMasterUIRenderer PRIVATE cxx_std_17)
    target_compile_definitions(SkellyMasterUIRenderer PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_REPORT_APP_USAGE=0)
    target_link_libraries(SkellyMasterUIRenderer PRIVATE
        SkellyMaster SkellyMasterPhotoAssets SkellCore::UIRenderer
        juce::juce_audio_utils juce::juce_audio_processors
        juce::juce_dsp juce::juce_gui_extra)
endif()
