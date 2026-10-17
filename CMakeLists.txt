cmake_minimum_required(VERSION 3.22)

project(MetalSubDrop VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static MSVC runtime: the plugin loads in any host without needing the VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(JUCE)

FetchContent_Declare(clap_juce_extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG        main
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(clap_juce_extensions)

juce_add_plugin(MetalSubDrop
    COMPANY_NAME                "IAMRAIN"
    BUNDLE_ID                   "ai.iamrain.metalsubdrop"
    PLUGIN_MANUFACTURER_CODE    Iamr
    PLUGIN_CODE                 SbDr
    PRODUCT_NAME                "Metal Sub Drop"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Instrument Synth
    FORMATS                     VST3 Standalone)

clap_juce_extensions_plugin(TARGET MetalSubDrop
    CLAP_ID       "ai.iamrain.metalsubdrop"
    CLAP_FEATURES instrument synthesizer mono)

target_sources(MetalSubDrop PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(MetalSubDrop PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(MetalSubDrop
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
