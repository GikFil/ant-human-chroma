cmake_minimum_required(VERSION 3.22)

project(antihuman_scale VERSION 1.0.0 LANGUAGES C CXX)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.5
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(AntihumanScale
    COMPANY_NAME "ant!human"
    BUNDLE_ID "audio.antihuman.scale"
    PLUGIN_MANUFACTURER_CODE Anth
    PLUGIN_CODE Scal
    PRODUCT_NAME "ant!human SCALE"
    DESCRIPTION "Spectral pitch quantizer for musical scales"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    FORMATS VST3 Standalone
)

juce_generate_juce_header(AntihumanScale)

target_sources(AntihumanScale PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
)

target_compile_features(AntihumanScale PRIVATE cxx_std_17)

target_compile_definitions(AntihumanScale
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(AntihumanScale
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
