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

# The UI uses the exact shared SkellCore skull palette and skin state.
FetchContent_Declare(SkellCoreShared
    GIT_REPOSITORY https://github.com/skellyruno/skellcore-vst-template.git
    GIT_TAG 1292336c095cde53ac17a9d6c8f0ec2042b08273
    SOURCE_SUBDIR SkellCore
    GIT_SHALLOW FALSE)
set(SKELLCORE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SKELLCORE_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SkellCoreShared)

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
# The photographic reference-kit header is optional while assets are staged.
# When all three files are present, embed them in BinaryData and render them
# once in the existing static-backdrop cache; never read from disk at runtime.
set(SKELLY_MASTER_HEADER_ASSETS 0)
set(SKELLY_MASTER_HEADER_FILES
    Source/UI/Assets/header_nameplate_green_blank.png
    Source/UI/Assets/logo_skull_header_left.png)
set(SKELLY_MASTER_HEADER_READY TRUE)
foreach(asset IN LISTS SKELLY_MASTER_HEADER_FILES)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${asset}")
        set(SKELLY_MASTER_HEADER_READY FALSE)
    endif()
endforeach()
if(SKELLY_MASTER_HEADER_READY)
    set(SKELLY_MASTER_HEADER_ASSETS 1)
endif()

set(SKELLY_MASTER_BINARY_ASSETS
    Source/UI/Assets/knob_photo.png
    Source/UI/Assets/meter_card.jpg
    Source/UI/Assets/irongrain.jpg
    Source/UI/Assets/greenpatina.jpg
    Source/UI/Assets/worn_graphite_chassis.jpg)
if(SKELLY_MASTER_HEADER_READY)
    list(APPEND SKELLY_MASTER_BINARY_ASSETS ${SKELLY_MASTER_HEADER_FILES})
endif()
juce_add_binary_data(SkellyMasterPhotoAssets SOURCES ${SKELLY_MASTER_BINARY_ASSETS})

juce_generate_juce_header(SkellyMaster)
target_sources(SkellyMaster PRIVATE
    Source/PluginProcessor.cpp Source/PluginProcessor.h
    Source/PluginEditor.cpp Source/PluginEditor.h
    Source/UI/StarSkullMark.h
    Source/UI/HardwareFinish.h
    Source/DSP/MasteringEngine.h
    Source/DSP/LoudnessMeter.h)
target_compile_features(SkellyMaster PRIVATE cxx_std_17)
target_compile_definitions(SkellyMaster PUBLIC JUCE_VST3_CAN_REPLACE_VST2=0)
target_compile_definitions(SkellyMaster PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_REPORT_APP_USAGE=0
    SKELLY_MASTER_HEADER_ASSETS=${SKELLY_MASTER_HEADER_ASSETS})
target_link_libraries(SkellyMaster PRIVATE
    SkellyMasterPhotoAssets SkellCore::UI
    juce::juce_audio_utils juce::juce_audio_processors
    juce::juce_dsp juce::juce_gui_extra)

# Portable DSP tests do not require JUCE/Windows or any audio interface.
enable_testing()
add_executable(SkellyDSPTests tests/test_mastering.cpp)
target_compile_features(SkellyDSPTests PRIVATE cxx_std_17)
add_test(NAME SkellyDSPTests COMMAND SkellyDSPTests)

# Independent diagnostic: confirms that a sample-peak ceiling alone cannot
# certify intersample true-peak compliance. No DSP behavior is modified.
add_executable(SkellyTruePeakGapTests tests/test_true_peak_gap.cpp)
target_compile_features(SkellyTruePeakGapTests PRIVATE cxx_std_17)
add_test(NAME SkellyTruePeakGapTests COMMAND SkellyTruePeakGapTests)

# Experimental audio coloration prototype: compiled/tested separately from the
# shipped mastering processing chain until real-music A/B is approved.
add_executable(SkellyFiveBandColorTests tests/test_five_band_color.cpp)
target_compile_features(SkellyFiveBandColorTests PRIVATE cxx_std_17)
add_test(NAME SkellyFiveBandColorTests COMMAND SkellyFiveBandColorTests)

# Reference chain's +2dB / 5kHz post-limiter presence is an original,
# optional prototype, not wired to the release audio path.
add_executable(SkellyPostLimiterPresenceTests tests/test_post_limiter_presence.cpp)
target_compile_features(SkellyPostLimiterPresenceTests PRIVATE cxx_std_17)
add_test(NAME SkellyPostLimiterPresenceTests COMMAND SkellyPostLimiterPresenceTests)

# Real JUCE editor + processor smoke tests on the Windows runner. Renders
# offscreen snapshots at multiple sizes and verifies controls/layout/host DSP.
# Link the plugin's shared-code target rather than recompiling the engine/UI.
if(WIN32)
    juce_add_console_app(SkellyUITests PRODUCT_NAME "Skelly Master UI Tests")
    target_sources(SkellyUITests PRIVATE tests/test_ui.cpp)
    target_compile_features(SkellyUITests PRIVATE cxx_std_17)
    target_compile_definitions(SkellyUITests PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_REPORT_APP_USAGE=0
        SKELLY_MASTER_HEADER_ASSETS=${SKELLY_MASTER_HEADER_ASSETS})
    target_link_libraries(SkellyUITests PRIVATE SkellyMaster
        SkellyMasterPhotoAssets SkellCore::UI
        juce::juce_audio_utils juce::juce_audio_processors
        juce::juce_dsp juce::juce_gui_extra)
    juce_generate_juce_header(SkellyUITests)
    add_test(NAME SkellyUITests COMMAND SkellyUITests)
    set_tests_properties(SkellyUITests PROPERTIES TIMEOUT 90)

    # REAL compiled VST3 render, not a linked processor simulation. Loads
    # the packaged plugin via JUCE's Windows VST3 host implementation and
    # processes a deterministic bass/transient/stereo waveform. Optional
    # WAV arguments permit local user-reference A/B without uploading audio.
    juce_add_console_app(SkellyVST3HostAudioTests PRODUCT_NAME "Skell Master VST3 Audio Test")
    target_sources(SkellyVST3HostAudioTests PRIVATE tests/test_vst3_host_audio.cpp)
    target_compile_features(SkellyVST3HostAudioTests PRIVATE cxx_std_17)
    target_compile_definitions(SkellyVST3HostAudioTests PRIVATE
        JUCE_PLUGINHOST_VST3=1 JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_REPORT_APP_USAGE=0)
    target_link_libraries(SkellyVST3HostAudioTests PRIVATE
        juce::juce_audio_utils juce::juce_audio_processors
        juce::juce_audio_formats juce::juce_dsp)
    juce_generate_juce_header(SkellyVST3HostAudioTests)
    add_test(NAME SkellyVST3HostAudioTests COMMAND SkellyVST3HostAudioTests)
    add_test(NAME SkellyVST3HostAudio48k COMMAND SkellyVST3HostAudioTests --rate 48000)
    add_test(NAME SkellyVST3HostAudio96k COMMAND SkellyVST3HostAudioTests --rate 96000)
    set_tests_properties(SkellyVST3HostAudioTests
        SkellyVST3HostAudio48k SkellyVST3HostAudio96k PROPERTIES TIMEOUT 180)
endif()


# Permanent optional SkellCore headless renderer. The executable links the
# actual plugin shared-code target and is never included in the shipped VST3.
option(SKELLY_MASTER_BUILD_UI_RENDERER "Build real editor with pinned SkellCore renderer" OFF)
if(SKELLY_MASTER_BUILD_UI_RENDERER)
    # SkellCoreShared is already loaded and is reused here.
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
