#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/LoFiChain.h"
#include "Parameters.h"
#include "Presets.h"

//==============================================================================
//  MIDI:  nota 36 (C1) = TAPESTOP (note-on aprieta, note-off suelta)
//         nota 37 (C#1) = DROPOUT (velocity escala la profundidad)
//==============================================================================
class LoFiBoxProcessor : public juce::AudioProcessor
{
public:
    static constexpr int kTapeStopNote = 36;
    static constexpr int kDropoutNote  = 37;

    LoFiBoxProcessor();
    ~LoFiBoxProcessor() override = default;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Lo-Fi Box"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    //==========================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    //  API para la GUI (message thread)
    juce::AudioProcessorValueTreeState apvts;

    /** index 0..5 = módulo, 6 = dado global. */
    void rollDie (int index);
    void toggleDieLock (int moduleIndex);

    std::array<std::atomic<int>, lofi::kNumModules + 1>      diceFaces;
    std::array<std::atomic<uint32_t>, lofi::kNumModules + 1> diceRollCount;
    std::array<std::atomic<bool>, lofi::kNumModules>         diceLocks;

    juce::String getPresetName() const;
    void setPresetName (const juce::String& n);
    bool saveUserPreset (const juce::File& file);
    bool loadUserPreset (const juce::File& file);

    // medidores / estado de transporte para la GUI
    std::atomic<float> meterRms  { 0.0f };
    std::atomic<float> meterPeak { 0.0f };
    std::atomic<float> hostBpm   { 120.0f };
    float getTapeSpeed() const noexcept           { return chain.getTapeSpeed(); }
    lofi::TapeStopModule::State getTapeState() const noexcept { return chain.getTapeState(); }
    float getDropoutActivity() const noexcept     { return chain.getDropoutActivity(); }
    bool  isMidiTapeStopHeld() const noexcept     { return midiTsHeld.load(); }

private:
    void readParameters (lofi::ChainParams& p, double bpm, int tsNum, int tsDen);
    juce::ValueTree makeUiStateTree() const;
    void restoreUiStateTree (const juce::ValueTree& ui);

    lofi::LoFiChain chain;
    lofi::ChainParams params;

    struct Raw { std::atomic<float>* p = nullptr; float get() const noexcept { return p->load (std::memory_order_relaxed); } };
    Raw noiseOn, noiseLevel, noiseDensity, noiseTone,
        wowOn, wowRate, wowDepth, flutterRate, flutterDepth, wowDrift,
        crushOn, crushBits, crushRate, crushMix,
        filterOn, filterHp, filterLp, filterRes,
        dropOn, dropDepth, dropMode, dropRate, dropTrigger,
        tsOn, tsTrigger, tsSync, tsDivision, tsTime, tsCurve, tsRestart,
        mix, output;

    bool prevDropParam = false;
    std::atomic<bool> midiTsHeld { false };
    int currentProgram = 0;
    juce::String presetName { "Default" };
    mutable juce::CriticalSection presetNameLock;
    juce::Random diceRng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoFiBoxProcessor)
};
