cmake_minimum_required(VERSION 3.24)
project(TagoClip VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(third_party/JUCE)

juce_add_plugin(TagoClip
    COMPANY_NAME "TagoBeats"
    BUNDLE_ID com.tagobeats.tagoclip
    PLUGIN_MANUFACTURER_CODE Tago
    PLUGIN_CODE TgCl
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "TagoClip"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(TagoClip PRIVATE
    plugin/PluginProcessor.cpp)

target_compile_definitions(TagoClip PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(TagoClip
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline render CLI for the golden comparison against the Python references.
# Dev tool only, opt in with -DTAGOCLIP_BUILD_TOOLS=ON.
option(TAGOCLIP_BUILD_TOOLS "Build the offline render CLI" OFF)
if(TAGOCLIP_BUILD_TOOLS)
    juce_add_console_app(TagoClipRender PRODUCT_NAME "TagoClipRender")
    target_sources(TagoClipRender PRIVATE tools/render_cli.cpp)
    target_compile_definitions(TagoClipRender PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    target_link_libraries(TagoClipRender PRIVATE
        juce::juce_audio_formats
        PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
endif()
