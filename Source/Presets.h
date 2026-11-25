#pragma once
//==============================================================================
//  Presets de fábrica + carpeta de presets de usuario (.lofibox = XML del estado)
//==============================================================================
#include "Parameters.h"

namespace lofi
{
int  getNumFactoryPresets();
juce::String getFactoryPresetName (int index);
/** Resetea todos los parámetros a default y aplica los valores del preset. */
void applyFactoryPreset (juce::AudioProcessorValueTreeState& apvts, int index);

juce::File getUserPresetFolder();       // ~/Music/Lo-Fi Box Presets (Mac) / Documents en otros
juce::Array<juce::File> getUserPresetFiles();
inline const char* kPresetExtension = ".lofibox";
} // namespace lofi
