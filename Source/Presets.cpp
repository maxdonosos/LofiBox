#include "Presets.h"

namespace lofi
{
namespace
{
    struct FactoryPreset
    {
        const char* name;
        std::vector<std::pair<const char*, float>> values;
    };

    const std::vector<FactoryPreset>& factory()
    {
        static const std::vector<FactoryPreset> presets
        {
            { "Init - Clean Tape", {
                { ID::noiseLevel, -50.0f }, { ID::noiseDensity, 0.6f }, { ID::noiseTone, 50.0f },
                { ID::wowRate, 0.5f }, { ID::wowDepth, 5.0f }, { ID::flutterRate, 6.0f }, { ID::flutterDepth, 1.5f }, { ID::wowDrift, 15.0f },
                { ID::crushOn, 0.0f },
                { ID::filterHp, 20.0f }, { ID::filterLp, 20000.0f }, { ID::filterRes, 0.0f } } },

            { "Dusty Vinyl", {
                { ID::noiseLevel, -36.0f }, { ID::noiseDensity, 9.0f }, { ID::noiseTone, 35.0f },
                { ID::wowRate, 0.55f }, { ID::wowDepth, 9.0f }, { ID::flutterRate, 5.0f }, { ID::flutterDepth, 0.8f }, { ID::wowDrift, 15.0f },
                { ID::crushBits, 12.0f }, { ID::crushRate, 22050.0f }, { ID::crushMix, 30.0f },
                { ID::filterHp, 55.0f }, { ID::filterLp, 9500.0f }, { ID::filterRes, 10.0f },
                { ID::dropMode, 1.0f }, { ID::dropRate, 2.0f }, { ID::dropDepth, 25.0f } } },

            { "Worn Cassette", {
                { ID::noiseLevel, -35.0f }, { ID::noiseDensity, 0.8f }, { ID::noiseTone, 70.0f },
                { ID::wowRate, 0.8f }, { ID::wowDepth, 20.0f }, { ID::flutterRate, 7.5f }, { ID::flutterDepth, 5.5f }, { ID::wowDrift, 55.0f },
                { ID::crushOn, 0.0f },
                { ID::filterHp, 85.0f }, { ID::filterLp, 7000.0f }, { ID::filterRes, 20.0f },
                { ID::dropMode, 1.0f }, { ID::dropRate, 8.0f }, { ID::dropDepth, 55.0f } } },

            { "AM Radio 1940", {
                { ID::noiseLevel, -31.0f }, { ID::noiseDensity, 3.0f }, { ID::noiseTone, 60.0f },
                { ID::wowRate, 0.3f }, { ID::wowDepth, 4.0f }, { ID::flutterDepth, 0.0f }, { ID::wowDrift, 10.0f },
                { ID::crushBits, 8.0f }, { ID::crushRate, 8000.0f }, { ID::crushMix, 65.0f },
                { ID::filterHp, 350.0f }, { ID::filterLp, 3200.0f }, { ID::filterRes, 45.0f },
                { ID::dropMode, 1.0f }, { ID::dropRate, 4.0f }, { ID::dropDepth, 35.0f } } },

            { "Lo-Fi Beats", {
                { ID::noiseLevel, -40.0f }, { ID::noiseDensity, 4.0f }, { ID::noiseTone, 45.0f },
                { ID::wowRate, 0.6f }, { ID::wowDepth, 16.0f }, { ID::flutterRate, 5.5f }, { ID::flutterDepth, 3.0f }, { ID::wowDrift, 35.0f },
                { ID::crushBits, 11.0f }, { ID::crushRate, 14000.0f }, { ID::crushMix, 45.0f },
                { ID::filterHp, 70.0f }, { ID::filterLp, 8500.0f }, { ID::filterRes, 15.0f },
                { ID::dropMode, 1.0f }, { ID::dropRate, 3.0f }, { ID::dropDepth, 30.0f },
                { ID::tsDivision, 4.0f }, { ID::tsCurve, 2.0f } } },

            { "Broken Walkman", {
                { ID::noiseLevel, -32.0f }, { ID::noiseDensity, 2.0f }, { ID::noiseTone, 80.0f },
                { ID::wowRate, 1.4f }, { ID::wowDepth, 38.0f }, { ID::flutterRate, 9.0f }, { ID::flutterDepth, 11.0f }, { ID::wowDrift, 85.0f },
                { ID::crushBits, 7.0f }, { ID::crushRate, 6000.0f }, { ID::crushMix, 50.0f },
                { ID::filterHp, 200.0f }, { ID::filterLp, 5000.0f }, { ID::filterRes, 35.0f },
                { ID::dropMode, 1.0f }, { ID::dropRate, 20.0f }, { ID::dropDepth, 80.0f },
                { ID::tsDivision, 2.0f }, { ID::tsCurve, 0.0f }, { ID::tsRestart, 1.0f } } },

            { "Tape Stop Only", {
                { ID::noiseOn, 0.0f }, { ID::wowOn, 0.0f }, { ID::crushOn, 0.0f }, { ID::filterOn, 0.0f }, { ID::dropOn, 0.0f },
                { ID::tsDivision, 3.0f }, { ID::tsCurve, 1.0f } } },
        };
        return presets;
    }
}

int getNumFactoryPresets() { return (int) factory().size(); }

juce::String getFactoryPresetName (int index)
{
    if (juce::isPositiveAndBelow (index, getNumFactoryPresets()))
        return factory()[(size_t) index].name;
    return {};
}

void applyFactoryPreset (juce::AudioProcessorValueTreeState& apvts, int index)
{
    if (! juce::isPositiveAndBelow (index, getNumFactoryPresets())) return;

    // 1) todo a default
    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->getDefaultValue());
            rp->endChangeGesture();
        }

    // 2) valores del preset
    for (auto& [id, value] : factory()[(size_t) index].values)
        setParamValue (apvts, id, value);
}

juce::File getUserPresetFolder()
{
   #if JUCE_MAC
    auto base = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
   #else
    auto base = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
   #endif
    auto dir = base.getChildFile ("Lo-Fi Box Presets");
    if (! dir.isDirectory()) dir.createDirectory();
    return dir;
}

juce::Array<juce::File> getUserPresetFiles()
{
    auto files = getUserPresetFolder().findChildFiles (juce::File::findFiles, false, juce::String ("*") + kPresetExtension);
    files.sort();
    return files;
}
} // namespace lofi
