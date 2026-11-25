#pragma once
#include "PluginProcessor.h"
#include "gui/LoFiLookAndFeel.h"
#include "gui/Components.h"

//==============================================================================
/** Vista principal diseñada a 1200 x 720 "puntos". El editor la escala con una
    transformación afín (vectorial => nítido a cualquier tamaño). */
class MainView : public juce::Component, private juce::Timer
{
public:
    static constexpr int W = 1200, H = 720;

    explicit MainView (LoFiBoxProcessor&);
    ~MainView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    lofi::TapeStopButton& getTapeStopButton() { return tsButton; }

private:
    void timerCallback() override;
    void buildBackground();
    void rebuildPresetList();
    void presetChosen();
    void stepPreset (int delta);
    void savePreset();
    void loadPresetFromFile();
    void updateTimeKnobVisibility();

    LoFiBoxProcessor& proc;
    juce::AudioProcessorValueTreeState& apvts;
    juce::Image background;

    // ---- header
    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, saveButton { "SAVE" }, loadButton { "LOAD" };
    lofi::DieButton globalDie;
    lofi::Caption globalDieCaption { "RANDOM ALL", juce::Justification::centredRight };
    juce::Array<juce::File> userPresetFiles;
    std::unique_ptr<juce::FileChooser> chooser;
    bool updatingPresetBox = false;

    // ---- paneles
    lofi::ModulePanel noisePanel { "01", "NOISE" }, wowPanel { "02", "WOW & FLUTTER" }, crushPanel { "03", "CRUSH" },
                      filterPanel { "04", "FILTER" }, dropPanel { "05", "DROPOUTS" }, masterPanel { "--", "MASTER" },
                      tsPanel { "06", "TAPESTOP" };

    // NOISE
    lofi::Knob noiseLevel, noiseDensity, noiseTone;
    // WOW
    lofi::Knob wowRate, wowDepth, flutRate, flutDepth, drift;
    lofi::Caption wowCap { "WOW", juce::Justification::centred }, flutCap { "FLUTTER", juce::Justification::centred };
    // CRUSH
    lofi::Knob crushBits, crushRate, crushMix;
    // FILTER
    lofi::Knob hp, lp, res;
    // DROPOUTS
    lofi::Knob dropDepth, dropRate;
    lofi::DropButton dropButton;
    lofi::SegmentedChoice dropMode;
    lofi::Caption dropModeCap { "TRIGGER" };
    // MASTER
    lofi::VUMeter vu;
    lofi::Knob mix, output;
    // TAPESTOP
    lofi::Cassette cassette;
    lofi::TapeStopButton tsButton;
    lofi::Knob tsDivision, tsTime;
    lofi::SegmentedChoice tsSync, tsCurve, tsRestart;
    lofi::Caption tsSyncCap { "TIME BASE" }, tsCurveCap { "CURVE" }, tsRestartCap { "RESTART" }, bpmCap { "", juce::Justification::centredRight };
    lofi::Caption tsHint { "", juce::Justification::centred };

    std::array<lofi::DieButton*, lofi::kNumModules> moduleDice {};
    std::array<uint32_t, lofi::kNumModules + 1> lastRollCount {};
    double lastTimerMs = 0.0;
    juce::String lastPresetName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainView)
};

//==============================================================================
class LoFiBoxEditor : public juce::AudioProcessorEditor
{
public:
    explicit LoFiBoxEditor (LoFiBoxProcessor&);
    ~LoFiBoxEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyStateChanged (bool isKeyDown) override;

private:
    lofi::LoFiLookAndFeel lnf;
    MainView view;
    juce::TooltipWindow tooltips { this, 600 };
    bool spaceDown = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoFiBoxEditor)
};
