cmake_minimum_required(VERSION 3.24)
project(TagoClip VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(TAGOCLIP_DEV_UI "Load the plugin UI from the Vite dev server (http://localhost:5173) instead of the bundled build" OFF)

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
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(TagoClip PRIVATE
    plugin/PluginProcessor.cpp
    plugin/PluginEditor.cpp)

# The bundled UI: ui/webui.zip is produced by scripts/build-ui.sh from the Vite build.
if(NOT TAGOCLIP_DEV_UI AND NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ui/webui.zip")
    message(FATAL_ERROR "ui/webui.zip missing. Run scripts/build-ui.sh first or configure with -DTAGOCLIP_DEV_UI=ON.")
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ui/webui.zip")
    juce_add_binary_data(TagoClipAssets SOURCES ui/webui.zip)
else()
    juce_add_binary_data(TagoClipAssets SOURCES cmake/empty_webui.zip)
endif()

target_compile_definitions(TagoClip PRIVATE
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    TAGOCLIP_DEV_UI=$<BOOL:${TAGOCLIP_DEV_UI}>)

if(WIN32)
    # NEEDS_WEBVIEW2 only links the WebView2 loader lib; this config macro is what
    # actually enables the WebView2 backend (and the resource provider) in juce_gui_extra.
    target_compile_definitions(TagoClip PRIVATE JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1)
endif()

target_link_libraries(TagoClip
    PRIVATE
        TagoClipAssets
        juce::juce_audio_utils
        juce::juce_gui_extra
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
