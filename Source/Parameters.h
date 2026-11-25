#pragma once
//==============================================================================
//  Parámetros (IDs, layout del APVTS) y randomizers con rangos musicales.
//==============================================================================
#include <juce_audio_processors/juce_audio_processors.h>

namespace lofi
{
namespace ID
{
    // NOISE
    inline constexpr const char* noiseOn       = "noise_on";
    inline constexpr const char* noiseLevel    = "noise_level";
    inline constexpr const char* noiseDensity  = "noise_density";
    inline constexpr const char* noiseTone     = "noise_tone";
    // WOW & FLUTTER
    inline constexpr const char* wowOn         = "wow_on";
    inline constexpr const char* wowRate       = "wow_rate";
    inline constexpr const char* wowDepth      = "wow_depth";
    inline constexpr const char* flutterRate   = "flutter_rate";
    inline constexpr const char* flutterDepth  = "flutter_depth";
    inline constexpr const char* wowDrift      = "wow_drift";
    // CRUSH
    inline constexpr const char* crushOn       = "crush_on";
    inline constexpr const char* crushBits     = "crush_bits";
    inline constexpr const char* crushRate     = "crush_rate";
    inline constexpr const char* crushMix      = "crush_mix";
    // FILTER
    inline constexpr const char* filterOn      = "filter_on";
    inline constexpr const char* filterHp      = "filter_hp";
    inline constexpr const char* filterLp      = "filter_lp";
    inline constexpr const char* filterRes     = "filter_res";
    // DROPOUTS
    inline constexpr const char* dropOn        = "drop_on";
    inline constexpr const char* dropDepth     = "drop_depth";
    inline constexpr const char* dropMode      = "drop_mode";
    inline constexpr const char* dropRate      = "drop_rate";
    inline constexpr const char* dropTrigger   = "drop_trigger";
    // TAPESTOP
    inline constexpr const char* tsOn          = "ts_on";
    inline constexpr const char* tsTrigger     = "ts_trigger";
    inline constexpr const char* tsSync        = "ts_sync";
    inline constexpr const char* tsDivision    = "ts_division";
    inline constexpr const char* tsTime        = "ts_time";
    inline constexpr const char* tsCurve       = "ts_curve";
    inline constexpr const char* tsRestart     = "ts_restart";
    // MASTER (nunca se randomizan)
    inline constexpr const char* mix           = "mix";
    inline constexpr const char* output        = "output";
}

enum class Module { Noise = 0, Wow, Crush, Filter, Dropout, TapeStop, NumModules };
constexpr int kNumModules = (int) Module::NumModules;
constexpr int kGlobalDie  = kNumModules;     // índice del dado global

/** Duración en beats de cada división de sync del tapestop (bar = compás). */
const juce::StringArray& tapeStopDivisionNames();
double tapeStopDivisionBeats (int index, int timeSigNum, int timeSigDen);

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

/** Randomiza los parámetros de un módulo dentro de rangos musicales.
    Debe llamarse desde el message thread (usa gestos de automatización).
    Nunca toca bypass, triggers, modos de trigger/reinicio, MIX ni OUTPUT. */
void randomizeModule (juce::AudioProcessorValueTreeState& apvts, Module m, juce::Random& rng);

/** Pone un parámetro (valor real, no normalizado) notificando al host. */
void setParamValue (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float realValue);
} // namespace lofi
