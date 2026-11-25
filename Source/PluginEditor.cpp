#include "PluginEditor.h"

using namespace lofi;

namespace
{
    juce::RangedAudioParameter& param (juce::AudioProcessorValueTreeState& s, const char* id)
    {
        auto* p = s.getParameter (id);
        jassert (p != nullptr);
        return *p;
    }
}

//==============================================================================
MainView::MainView (LoFiBoxProcessor& p)
    : proc (p), apvts (p.apvts),
      noiseLevel   (apvts, ID::noiseLevel,   "LEVEL"),
      noiseDensity (apvts, ID::noiseDensity, "CRACKLE"),
      noiseTone    (apvts, ID::noiseTone,    "TONE"),
      wowRate      (apvts, ID::wowRate,      "RATE"),
      wowDepth     (apvts, ID::wowDepth,     "DEPTH"),
      flutRate     (apvts, ID::flutterRate,  "RATE"),
      flutDepth    (apvts, ID::flutterDepth, "DEPTH"),
      drift        (apvts, ID::wowDrift,     "DRIFT"),
      crushBits    (apvts, ID::crushBits,    "BITS"),
      crushRate    (apvts, ID::crushRate,    "RATE"),
      crushMix     (apvts, ID::crushMix,     "MIX"),
      hp           (apvts, ID::filterHp,     "LOW CUT"),
      lp           (apvts, ID::filterLp,     "HIGH CUT"),
      res          (apvts, ID::filterRes,    "RESO"),
      dropDepth    (apvts, ID::dropDepth,    "DEPTH"),
      dropRate     (apvts, ID::dropRate,     "PER MIN"),
      dropButton   (param (apvts, ID::dropTrigger)),
      dropMode     (param (apvts, ID::dropMode), { "MANUAL", "AUTO" }),
      mix          (apvts, ID::mix,          "MIX"),
      output       (apvts, ID::output,       "OUTPUT"),
      tsButton     (param (apvts, ID::tsTrigger)),
      tsDivision   (apvts, ID::tsDivision,   "TIME"),
      tsTime       (apvts, ID::tsTime,       "TIME"),
      tsSync       (param (apvts, ID::tsSync), { "SYNC", "FREE" }, true),
      tsCurve      (param (apvts, ID::tsCurve), { "LIN", "EXP", "S" }),
      tsRestart    (param (apvts, ID::tsRestart), { "RELEASE", "RETRIG" })
{
    setOpaque (true);
    buildBackground();

    // ---------------- header
    addAndMakeVisible (presetBox);
    presetBox.setTooltip ("Presets (factory + user)");
    presetBox.onChange = [this] { presetChosen(); };
    for (auto* b : { &prevButton, &nextButton, &saveButton, &loadButton }) addAndMakeVisible (*b);
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (+1); };
    saveButton.onClick = [this] { savePreset(); };
    loadButton.onClick = [this] { loadPresetFromFile(); };
    saveButton.setTooltip ("Save user preset (.lofibox)");
    loadButton.setTooltip ("Load preset file");

    addAndMakeVisible (globalDie);
    globalDie.setBig (true);
    globalDie.setLockable (false);
    globalDie.setTooltip ("Randomize the whole chain (never touches MIX / OUTPUT, skips locked dice)");
    globalDie.onRoll = [this] { proc.rollDie (kGlobalDie); };
    addAndMakeVisible (globalDieCaption);
    globalDieCaption.colour = Palette::creamDim;

    // ---------------- paneles con dado + power
    struct Def { ModulePanel* panel; const char* powerId; Module m; };
    const Def defs[] = {
        { &noisePanel,  ID::noiseOn,  Module::Noise },
        { &wowPanel,    ID::wowOn,    Module::Wow },
        { &crushPanel,  ID::crushOn,  Module::Crush },
        { &filterPanel, ID::filterOn, Module::Filter },
        { &dropPanel,   ID::dropOn,   Module::Dropout },
        { &tsPanel,     ID::tsOn,     Module::TapeStop } };

    for (auto& d : defs)
    {
        addAndMakeVisible (*d.panel);
        const int idx = (int) d.m;
        d.panel->hasDie = true;
        d.panel->addAndMakeVisible (d.panel->die);
        d.panel->die.onRoll       = [this, idx] { proc.rollDie (idx); };
        d.panel->die.onToggleLock = [this, idx] { proc.toggleDieLock (idx); };
        moduleDice[(size_t) idx] = &d.panel->die;
    }
    addAndMakeVisible (masterPanel);

    noisePanel.addContent (noiseLevel); noisePanel.addContent (noiseDensity); noisePanel.addContent (noiseTone);

    for (auto* k : { &wowRate, &wowDepth, &flutRate, &flutDepth, &drift }) wowPanel.addContent (*k);
    wowPanel.addContent (wowCap); wowPanel.addContent (flutCap);
    wowCap.colour = flutCap.colour = Palette::orange;

    crushPanel.addContent (crushBits); crushPanel.addContent (crushRate); crushPanel.addContent (crushMix);
    filterPanel.addContent (hp); filterPanel.addContent (lp); filterPanel.addContent (res);

    dropPanel.addContent (dropDepth); dropPanel.addContent (dropRate); dropPanel.addContent (dropButton);
    dropPanel.addContent (dropMode);  dropPanel.addContent (dropModeCap);
    dropButton.setTooltip ("Trigger a dropout  (MIDI note C#1 / 37, velocity = depth)");

    masterPanel.addContent (vu); masterPanel.addContent (mix); masterPanel.addContent (output);

    tsPanel.addContent (cassette); tsPanel.addContent (tsButton);
    tsPanel.addContent (tsDivision); tsPanel.addContent (tsTime);
    tsPanel.addContent (tsSync); tsPanel.addContent (tsCurve); tsPanel.addContent (tsRestart);
    tsPanel.addContent (tsSyncCap); tsPanel.addContent (tsCurveCap); tsPanel.addContent (tsRestartCap);
    tsPanel.addContent (bpmCap); tsPanel.addContent (tsHint);
    bpmCap.colour = Palette::orange;
    tsHint.colour = Palette::creamFaint;
    tsHint.height = 9.5f;
    tsButton.setTooltip ("TAPE STOP  (MIDI note C1 / 36  -  space bar in standalone)");
    tsSync.onChange = [this] (int) { updateTimeKnobVisibility(); };

    // power (después de addContent para que el "dim" aplique a todo)
    noisePanel.attachPower  (apvts, ID::noiseOn);
    wowPanel.attachPower    (apvts, ID::wowOn);
    crushPanel.attachPower  (apvts, ID::crushOn);
    filterPanel.attachPower (apvts, ID::filterOn);
    dropPanel.attachPower   (apvts, ID::dropOn);
    tsPanel.attachPower     (apvts, ID::tsOn);

    updateTimeKnobVisibility();
    rebuildPresetList();

    for (int i = 0; i <= kNumModules; ++i)
        lastRollCount[(size_t) i] = proc.diceRollCount[(size_t) i].load();
    for (int i = 0; i < kNumModules; ++i)
    {
        moduleDice[(size_t) i]->setFace (proc.diceFaces[(size_t) i].load(), false);
        moduleDice[(size_t) i]->setLocked (proc.diceLocks[(size_t) i].load());
    }
    globalDie.setFace (proc.diceFaces[(size_t) kGlobalDie].load(), false);

    setSize (W, H);
    lastTimerMs = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (60);
}

MainView::~MainView() { stopTimer(); }

//==============================================================================
//  Layout (coordenadas de diseño 1200 x 720)
//==============================================================================
void MainView::resized()
{
    // ---- header
    presetBox.setBounds (468, 20, 264, 32);
    prevButton.setBounds (430, 22, 32, 28);
    nextButton.setBounds (738, 22, 32, 28);
    saveButton.setBounds (782, 22, 62, 28);
    loadButton.setBounds (850, 22, 62, 28);
    globalDie.setBounds (1134, 14, 44, 44);
    globalDieCaption.setBounds (1010, 26, 118, 20);

    // ---- grilla de módulos
    noisePanel.setBounds  (16, 84, 236, 300);
    wowPanel.setBounds    (264, 84, 304, 300);
    crushPanel.setBounds  (580, 84, 224, 300);
    filterPanel.setBounds (16, 396, 236, 300);
    dropPanel.setBounds   (264, 396, 304, 300);
    masterPanel.setBounds (580, 396, 224, 300);
    tsPanel.setBounds     (816, 84, 368, 612);

    const int bigW = 104, bigH = 122, smW = 80, smH = 98;

    // NOISE: nivel grande arriba, crackle + tone abajo
    noiseLevel.setBounds   (66, 54, bigW, bigH);
    noiseDensity.setBounds (24, 188, smW, smH);
    noiseTone.setBounds    (132, 188, smW, smH);

    // WOW: columna wow / columna flutter / drift
    wowCap.setBounds  (16, 52, smW, 14);
    flutCap.setBounds (108, 52, smW, 14);
    wowRate.setBounds   (16, 68, smW, smH);
    wowDepth.setBounds  (16, 180, smW, smH);
    flutRate.setBounds  (108, 68, smW, smH);
    flutDepth.setBounds (108, 180, smW, smH);
    drift.setBounds     (194, 118, bigW, bigH);

    // CRUSH
    crushBits.setBounds (22, 60, smW, smH);
    crushRate.setBounds (122, 60, smW, smH);
    crushMix.setBounds  (60, 176, bigW, bigH);

    // FILTER
    hp.setBounds  (24, 60, smW, smH);
    lp.setBounds  (132, 60, smW, smH);
    res.setBounds (66, 176, bigW, bigH);

    // DROPOUTS
    dropDepth.setBounds   (20, 54, bigW, bigH);
    dropModeCap.setBounds (22, 196, 120, 14);
    dropMode.setBounds    (20, 214, 124, 28);
    dropButton.setBounds  (160, 50, 128, 128);
    dropRate.setBounds    (184, 188, smW, smH);

    // MASTER
    vu.setBounds     (12, 52, 200, 124);
    mix.setBounds    (24, 188, smW, smH);
    output.setBounds (120, 188, smW, smH);

    // TAPESTOP: cassette, botón gigante, controles
    cassette.setBounds  (30, 52, 308, 192);
    tsButton.setBounds  (76, 238, 216, 216);
    tsHint.setBounds    (20, 448, 328, 14);
    tsDivision.setBounds (18, 472, bigW, bigH);
    tsTime.setBounds     (18, 472, bigW, bigH);

    const int cx = 140, cw = 208;
    tsSyncCap.setBounds    (cx + 2, 470, 120, 14);
    bpmCap.setBounds       (cx + cw - 90, 470, 90, 14);
    tsSync.setBounds       (cx, 486, cw, 26);
    tsCurveCap.setBounds   (cx + 2, 520, 120, 14);
    tsCurve.setBounds      (cx, 536, cw, 26);
    tsRestartCap.setBounds (cx + 2, 570, 120, 14);
    tsRestart.setBounds    (cx, 586, cw, 26);
}

//==============================================================================
//  Fondo texturizado (generado una vez)
//==============================================================================
void MainView::buildBackground()
{
    background = juce::Image (juce::Image::RGB, W, H, true);
    {
        juce::Graphics g (background);
        juce::ColourGradient grad (Palette::bgLight, W * 0.45f, H * 0.35f, Palette::bg.darker (0.35f), 0, H, true);
        g.setGradientFill (grad);
        g.fillAll();
    }
    // grano + líneas horizontales muy sutiles (textura de cinta)
    juce::Random rng (1979);
    juce::Image::BitmapData bd (background, juce::Image::BitmapData::readWrite);
    for (int y = 0; y < H; ++y)
    {
        const float line = (y % 3 == 0) ? -2.0f : 0.0f;
        for (int x = 0; x < W; ++x)
        {
            auto c = bd.getPixelColour (x, y);
            const float n = (rng.nextFloat() - 0.5f) * 9.0f + line;
            auto adj = [n] (juce::uint8 v) { return (juce::uint8) juce::jlimit (0, 255, (int) v + (int) n); };
            bd.setPixelColour (x, y, juce::Colour (adj (c.getRed()), adj (c.getGreen()), adj (c.getBlue())));
        }
    }
}

void MainView::paint (juce::Graphics& g)
{
    g.drawImageAt (background, 0, 0);
    auto& lnf = getLnf (*this);

    // viñeta
    juce::ColourGradient vig (juce::Colour (0x00000000), W * 0.5f, H * 0.45f, juce::Colour (0x8c000000), 0, 0, true);
    g.setGradientFill (vig);
    g.fillRect (0, 0, W, H);

    // ---- logo
    g.setFont (lnf.titleFont (40.0f));
    g.setColour (Palette::cream);
    g.drawText ("LO-FI", 24, 12, 130, 44, juce::Justification::centredLeft);
    g.setColour (Palette::orange);
    g.drawText ("BOX", 140, 12, 100, 44, juce::Justification::centredLeft);
    g.setFont (lnf.labelFont (9.5f));
    g.setColour (Palette::creamDim);
    g.drawText ("TAPE  TEXTURE  MACHINE", 26, 52, 260, 14, juce::Justification::centredLeft);

    // franjas retro diagonales
    const juce::Colour stripes[] = { Palette::orangeHot, Palette::orange, Palette::rust, Palette::brown };
    for (int i = 0; i < 4; ++i)
    {
        juce::Path p;
        const float x = 236.0f + (float) i * 14.0f;
        p.addQuadrilateral (x, 58, x + 10, 58, x + 34, 18, x + 24, 18);
        g.setColour (stripes[i]);
        g.fillPath (p);
    }

    // separador del header
    g.setColour (juce::Colour (0xff0e0b09));
    g.fillRect (16, 72, W - 32, 1);
    g.setColour (juce::Colour (0x10ffffff));
    g.fillRect (16, 73, W - 32, 1);

    // pie
    g.setFont (lnf.labelFont (9.0f));
    g.setColour (Palette::creamFaint);
    g.drawText ("MIDI   C1 = TAPESTOP   C#1 = DROP      RIGHT-CLICK A DIE TO LOCK IT      DOUBLE-CLICK A KNOB TO RESET",
                16, 700, W - 32, 16, juce::Justification::centred);
}

//==============================================================================
//  Animación / sincronización con el procesador (60 Hz)
//==============================================================================
void MainView::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = (float) juce::jlimit (0.001, 0.1, (now - lastTimerMs) * 0.001);
    lastTimerMs = now;

    vu.update (proc.meterRms.load(), proc.meterPeak.exchange (0.0f), dt);
    cassette.advance (proc.getTapeSpeed(), dt);

    const bool toggleMode = tsRestart.getSelected() == 1;
    tsButton.setVisualState ((int) proc.getTapeState(), proc.getTapeSpeed(), proc.isMidiTapeStopHeld(), toggleMode);
    tsHint.setText (toggleMode ? "TAP TO STOP  /  TAP AGAIN TO RESTART" : "HOLD TO STOP  /  RELEASE TO RESTART");
    dropButton.setActivity (proc.getDropoutActivity());

    const bool autoMode = dropMode.getSelected() == 1;
    dropRate.setEnabled (autoMode);

    if (tsSync.getSelected() == 0)
        bpmCap.setText (juce::String (proc.hostBpm.load(), 1) + " BPM");
    else
        bpmCap.setText ({});

    // dados (animan si cambió el contador de tiradas: click propio, dado global o preset)
    for (int i = 0; i <= kNumModules; ++i)
    {
        const auto rc = proc.diceRollCount[(size_t) i].load();
        if (rc != lastRollCount[(size_t) i])
        {
            lastRollCount[(size_t) i] = rc;
            auto* die = (i == kGlobalDie) ? &globalDie : moduleDice[(size_t) i];
            die->setFace (proc.diceFaces[(size_t) i].load(), true);
        }
        if (i < kNumModules) moduleDice[(size_t) i]->setLocked (proc.diceLocks[(size_t) i].load());
    }

    const auto name = proc.getPresetName();
    if (name != lastPresetName)
    {
        lastPresetName = name;
        updatingPresetBox = true;
        presetBox.setText (name, juce::dontSendNotification);
        updatingPresetBox = false;
    }
}

void MainView::updateTimeKnobVisibility()
{
    const bool sync = tsSync.getSelected() == 0;
    tsDivision.setVisible (sync);
    tsTime.setVisible (! sync);
}

//==============================================================================
//  Presets
//==============================================================================
void MainView::rebuildPresetList()
{
    updatingPresetBox = true;
    presetBox.clear (juce::dontSendNotification);
    presetBox.addSectionHeading ("FACTORY");
    for (int i = 0; i < getNumFactoryPresets(); ++i)
        presetBox.addItem (getFactoryPresetName (i), i + 1);

    userPresetFiles = getUserPresetFiles();
    if (! userPresetFiles.isEmpty())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading ("USER");
        for (int i = 0; i < userPresetFiles.size(); ++i)
            presetBox.addItem (userPresetFiles[i].getFileNameWithoutExtension(), 1000 + i);
    }
    presetBox.setText (proc.getPresetName(), juce::dontSendNotification);
    lastPresetName = proc.getPresetName();
    updatingPresetBox = false;
}

void MainView::presetChosen()
{
    if (updatingPresetBox) return;
    const int id = presetBox.getSelectedId();
    if (id <= 0) return;
    if (id < 1000) proc.setCurrentProgram (id - 1);
    else if (juce::isPositiveAndBelow (id - 1000, userPresetFiles.size()))
        proc.loadUserPreset (userPresetFiles[id - 1000]);
}

void MainView::stepPreset (int delta)
{
    juce::Array<int> ids;
    for (int i = 0; i < presetBox.getNumItems(); ++i) ids.add (presetBox.getItemId (i));
    if (ids.isEmpty()) return;
    int idx = ids.indexOf (presetBox.getSelectedId());
    if (idx < 0)
    {
        // buscar por nombre (el combo puede mostrar texto sin selección)
        for (int i = 0; i < presetBox.getNumItems(); ++i)
            if (presetBox.getItemText (i) == proc.getPresetName()) idx = i;
    }
    idx = (idx < 0) ? 0 : (idx + delta + ids.size()) % ids.size();
    presetBox.setSelectedId (ids[idx], juce::sendNotificationSync);
}

void MainView::savePreset()
{
    chooser = std::make_unique<juce::FileChooser> ("Save Lo-Fi Box preset",
                                                   getUserPresetFolder().getChildFile (proc.getPresetName() + kPresetExtension),
                                                   juce::String ("*") + kPresetExtension);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f == juce::File()) return;
                              f = f.withFileExtension (kPresetExtension);
                              if (proc.saveUserPreset (f)) rebuildPresetList();
                          });
}

void MainView::loadPresetFromFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Load Lo-Fi Box preset", getUserPresetFolder(),
                                                   juce::String ("*") + kPresetExtension);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f.existsAsFile()) proc.loadUserPreset (f);
                          });
}

//==============================================================================
//  Editor: escala la vista manteniendo la proporción
//==============================================================================
LoFiBoxEditor::LoFiBoxEditor (LoFiBoxProcessor& p)
    : AudioProcessorEditor (&p), view (p)
{
    setLookAndFeel (&lnf);
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);   // popups / file choosers
    addAndMakeVisible (view);
    setResizable (true, true);
    setResizeLimits (840, 504, 1800, 1080);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) MainView::W / (double) MainView::H);
    setWantsKeyboardFocus (true);
    setSize (MainView::W, MainView::H);
}

LoFiBoxEditor::~LoFiBoxEditor()
{
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void LoFiBoxEditor::paint (juce::Graphics& g) { g.fillAll (Palette::bg); }

void LoFiBoxEditor::resized()
{
    const float s = (float) getWidth() / (float) MainView::W;
    view.setBounds (0, 0, MainView::W, MainView::H);
    view.setTransform (juce::AffineTransform::scale (s));
}

bool LoFiBoxEditor::keyStateChanged (bool)
{
    // Barra espaciadora = TAPE STOP (solo en la app standalone; en un DAW es play/stop)
    if (! juce::JUCEApplicationBase::isStandaloneApp()) return false;
    const bool down = juce::KeyPress::isKeyCurrentlyDown (juce::KeyPress::spaceKey);
    if (down != spaceDown)
    {
        spaceDown = down;
        view.getTapeStopButton().press (down);
        return true;
    }
    return false;
}
