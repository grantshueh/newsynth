cmake_minimum_required(VERSION 3.22)

project(NewSynth VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(NewSynth
    COMPANY_NAME "AloBoi"
    BUNDLE_ID com.aloboi.newsynth
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE AlBo
    PLUGIN_CODE NsJi
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "NewSynth")

target_sources(NewSynth PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Engine/Wavetable.cpp)

target_include_directories(NewSynth PRIVATE Source)

target_compile_definitions(NewSynth PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(NewSynth
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

# Headless unit tests for the tuning engine (pure C++, no JUCE)
add_executable(TuningTests Tests/TuningTests.cpp)
target_include_directories(TuningTests PRIVATE Source)

# Headless end-to-end render test (full voice engine, measures output pitch)
juce_add_console_app(RenderTest PRODUCT_NAME "RenderTest")
target_sources(RenderTest PRIVATE
    Tests/RenderTest.cpp
    Source/Engine/Wavetable.cpp)
target_include_directories(RenderTest PRIVATE Source)
target_link_libraries(RenderTest
    PRIVATE
        juce::juce_audio_basics
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags)
