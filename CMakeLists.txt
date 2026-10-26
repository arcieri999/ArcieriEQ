cmake_minimum_required(VERSION 3.22)
project(ArcieriEQ VERSION 1.0.0)

# Clona JUCE dentro de esta carpeta:  git clone --depth 1 https://github.com/juce-framework/JUCE.git
add_subdirectory(JUCE)

juce_add_plugin(ArcieriEQ
    COMPANY_NAME "Arcieri"
    PLUGIN_MANUFACTURER_CODE Arci
    PLUGIN_CODE Arcq
    FORMATS VST3 Standalone
    PRODUCT_NAME "Arcieri EQ"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(ArcieriEQAssets SOURCES Assets/logo.png)

target_sources(ArcieriEQ PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(ArcieriEQ PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(ArcieriEQ
    PRIVATE
        ArcieriEQAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
