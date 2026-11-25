cmake_minimum_required(VERSION 3.22)

# --- Mac: universal binary (Apple Silicon + Intel) y target mínimo.
#     Se puede sobreescribir: -DCMAKE_OSX_ARCHITECTURES=arm64
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Mac architectures")

project(LoFiBox VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS YES)

#------------------------------------------------------------------------------
# JUCE: usa una copia local si se pasa -DJUCE_DIR=/ruta/a/JUCE,
# si no, la descarga automáticamente (JUCE 8.0.10; también compila con 7.0.x).
#------------------------------------------------------------------------------
option(LOFIBOX_COPY_AFTER_BUILD "Copiar plugins a las carpetas del sistema al compilar" ON)
set(JUCE_DIR "" CACHE PATH "Ruta a un checkout local de JUCE (opcional)")
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.10
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

#------------------------------------------------------------------------------
# Fuentes embebidas (OFL): Righteous (títulos) + Space Mono (labels/valores)
#------------------------------------------------------------------------------
juce_add_binary_data(LoFiBoxAssets
    HEADER_NAME LoFiBoxAssets.h
    NAMESPACE   LoFiBoxAssets
    SOURCES
        Resources/Fonts/Righteous-Regular.ttf
        Resources/Fonts/SpaceMono-Bold.ttf
        Resources/Fonts/SpaceMono-Regular.ttf)

#------------------------------------------------------------------------------
# Plugin: AU + VST3 + Standalone
#------------------------------------------------------------------------------
if(APPLE)
    set(LOFIBOX_FORMATS AU VST3 Standalone)
else()
    set(LOFIBOX_FORMATS VST3 Standalone)
endif()

juce_add_plugin(LoFiBox
    COMPANY_NAME                "LoFiBox Audio"
    COMPANY_WEBSITE             "https://example.com"
    BUNDLE_ID                   com.lofiboxaudio.lofibox
    PLUGIN_MANUFACTURER_CODE    Lfbx
    PLUGIN_CODE                 LfB1
    PRODUCT_NAME                "Lo-Fi Box"
    FORMATS                     ${LOFIBOX_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    AU_MAIN_TYPE                kAudioUnitType_MusicEffect
    VST3_CATEGORIES             Fx Distortion
    COPY_PLUGIN_AFTER_BUILD     ${LOFIBOX_COPY_AFTER_BUILD})

target_sources(LoFiBox PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Parameters.cpp
    Source/Presets.cpp
    Source/gui/LoFiLookAndFeel.cpp
    Source/gui/Components.cpp)

target_include_directories(LoFiBox PRIVATE Source)

target_compile_definitions(LoFiBox PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(LoFiBox
    PRIVATE
        LoFiBoxAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

#------------------------------------------------------------------------------
# App de consola para probar el DSP sin host (render a WAV + null test)
#------------------------------------------------------------------------------
juce_add_console_app(LoFiBoxDSPTest PRODUCT_NAME "LoFiBoxDSPTest")
target_sources(LoFiBoxDSPTest PRIVATE tools/DSPTest.cpp)
target_compile_definitions(LoFiBoxDSPTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(LoFiBoxDSPTest
    PRIVATE juce::juce_audio_formats juce::juce_dsp
    PUBLIC  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
