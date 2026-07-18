cmake_minimum_required(VERSION 3.22)
project(PitchWrench VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Fetch JUCE ──────────────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.3)
FetchContent_MakeAvailable(juce)

# ── Bundle WebUI as BinaryData ───────────────────────────────────────────────
juce_add_binary_data(PitchWrenchWebUI 
    HEADER_NAME "PitchWrenchWebUIBinaryData.h"
    NAMESPACE PitchWrenchWebUIBinaryData
    SOURCES
    WebUI/index.html
    WebUI/style.css
    WebUI/app.js
    WebUI/logo.png)

# ── Plugin target ─────────────────────────────────────────────────────────────
juce_add_plugin(PitchWrench
    COMPANY_NAME            "Pulverine Audio"
    COMPANY_WEBSITE         "https://pulverine.audio"
    BUNDLE_ID               "audio.pulverine.pitchwrench"
    PLUGIN_MANUFACTURER_CODE Pulv
    PLUGIN_CODE             PwrX
    FORMATS                 VST3 AU Standalone
    PRODUCT_NAME            "Pitch Wrench"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES         "Fx" "Pitch Shift"
    AU_MAIN_TYPE            kAudioUnitType_Effect)

# ── Universal Binary (macOS) ──────────────────────────────────────────────────
if(APPLE)
    set_target_properties(PitchWrench PROPERTIES
        OSX_ARCHITECTURES "arm64;x86_64")
endif()

juce_generate_juce_header(PitchWrench)

# ── Sources ───────────────────────────────────────────────────────────────────
target_sources(PitchWrench PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PitchWrenchModule.cpp)

target_include_directories(PitchWrench PRIVATE Source)

# ── JUCE modules ─────────────────────────────────────────────────────────────
target_compile_definitions(PitchWrench PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(PitchWrench PRIVATE
    PitchWrenchWebUI
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_extra        # WebBrowserComponent
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
