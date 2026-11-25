#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace lofi;

LoFiBoxProcessor::LoFiBoxProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "LoFiBoxState", createParameterLayout()),
      diceRng (juce::Time::currentTimeMillis())
{
    auto bind = [this] (Raw& r, const char* id) { r.p = apvts.getRawParameterValue (id); jassert (r.p != nullptr); };
    bind (noiseOn, ID::noiseOn);   bind (noiseLevel, ID::noiseLevel); bind (noiseDensity, ID::noiseDensity); bind (noiseTone, ID::noiseTone);
    bind (wowOn, ID::wowOn);       bind (wowRate, ID::wowRate);       bind (wowDepth, ID::wowDepth);
    bind (flutterRate, ID::flutterRate); bind (flutterDepth, ID::flutterDepth); bind (wowDrift, ID::wowDrift);
    bind (crushOn, ID::crushOn);   bind (crushBits, ID::crushBits);   bind (crushRate, ID::crushRate); bind (crushMix, ID::crushMix);
    bind (filterOn, ID::filterOn); bind (filterHp, ID::filterHp);     bind (filterLp, ID::filterLp);   bind (filterRes, ID::filterRes);
    bind (dropOn, ID::dropOn);     bind (dropDepth, ID::dropDepth);   bind (dropMode, ID::dropMode);   bind (dropRate, ID::dropRate);
    bind (dropTrigger, ID::dropTrigger);
    bind (tsOn, ID::tsOn);         bind (tsTrigger, ID::tsTrigger);   bind (tsSync, ID::tsSync);       bind (tsDivision, ID::tsDivision);
    bind (tsTime, ID::tsTime);     bind (tsCurve, ID::tsCurve);       bind (tsRestart, ID::tsRestart);
    bind (mix, ID::mix);           bind (output, ID::output);

    for (int i = 0; i <= kNumModules; ++i) { diceFaces[(size_t) i] = 1 + (i % 6); diceRollCount[(size_t) i] = 0; }
    for (auto& l : diceLocks) l = false;
}

//==============================================================================
bool LoFiBoxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void LoFiBoxProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int nch = juce::jmax (1, getTotalNumOutputChannels());
    chain.prepare (sampleRate, juce::jmax (16, samplesPerBlock), nch);
    readParameters (params, 120.0, 4, 4);
    chain.setParams (params);
    chain.reset();
    setLatencySamples (chain.getLatencySamples());
    prevDropParam = dropTrigger.get() > 0.5f;
}

//==============================================================================
void LoFiBoxProcessor::readParameters (ChainParams& p, double bpm, int tsNum, int tsDen)
{
    p.noise.enabled = noiseOn.get() > 0.5f;
    p.noise.levelDb = noiseLevel.get();
    p.noise.density = noiseDensity.get();
    p.noise.tone    = noiseTone.get() * 0.01f;

    p.wow.enabled      = wowOn.get() > 0.5f;
    p.wow.wowRate      = wowRate.get();
    p.wow.wowDepth     = wowDepth.get();
    p.wow.flutterRate  = flutterRate.get();
    p.wow.flutterDepth = flutterDepth.get();
    p.wow.drift        = wowDrift.get() * 0.01f;

    p.crush.enabled = crushOn.get() > 0.5f;
    p.crush.bits    = crushBits.get();
    p.crush.rateHz  = crushRate.get();
    p.crush.mix     = crushMix.get() * 0.01f;

    p.filter.enabled = filterOn.get() > 0.5f;
    p.filter.hpHz    = filterHp.get();
    p.filter.lpHz    = filterLp.get();
    p.filter.res     = filterRes.get() * 0.01f;

    p.dropout.enabled       = dropOn.get() > 0.5f;
    p.dropout.depth         = dropDepth.get() * 0.01f;
    p.dropout.mode          = dropMode.get() > 0.5f ? DropoutModule::Mode::Auto : DropoutModule::Mode::Manual;
    p.dropout.ratePerMinute = dropRate.get();

    p.tapestop.enabled = tsOn.get() > 0.5f;
    if (tsSync.get() > 0.5f)
    {
        const double beats = tapeStopDivisionBeats (juce::roundToInt (tsDivision.get()), tsNum, tsDen);
        p.tapestop.timeSec = (float) (beats * 60.0 / juce::jlimit (20.0, 400.0, bpm));
    }
    else
        p.tapestop.timeSec = tsTime.get() * 0.001f;
    p.tapestop.curve   = (TapeStopModule::Curve) juce::jlimit (0, 2, juce::roundToInt (tsCurve.get()));
    p.tapestop.restart = tsRestart.get() > 0.5f ? TapeStopModule::Restart::OnRetrigger : TapeStopModule::Restart::OnRelease;

    p.mix      = mix.get() * 0.01f;
    p.outputDb = output.get();
}

void LoFiBoxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, numSamples);

    // --- tempo del host
    double bpm = 120.0; int num = 4, den = 4;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto t = pos->getTimeSignature()) { num = t->numerator; den = t->denominator; }
        }
    hostBpm.store ((float) bpm, std::memory_order_relaxed);

    readParameters (params, bpm, num, den);
    chain.setParams (params);

    // --- triggers desde parámetros (botones GUI / automatización)
    const bool dropParam = dropTrigger.get() > 0.5f;
    if (dropParam && ! prevDropParam) chain.triggerDropout (1.0f);
    prevDropParam = dropParam;

    const bool tsParam = tsTrigger.get() > 0.5f;
    chain.setTapeStopPressed (tsParam || midiTsHeld.load());

    // --- MIDI sample-accurate: procesamos sub-bloques entre eventos
    int pos = 0;
    for (const auto meta : midi)
    {
        const int s = juce::jlimit (0, numSamples, meta.samplePosition);
        if (s > pos) { chain.process (buffer, pos, s - pos); pos = s; }

        const auto m = meta.getMessage();
        if (m.isNoteOn())
        {
            if (m.getNoteNumber() == kTapeStopNote) midiTsHeld = true;
            else if (m.getNoteNumber() == kDropoutNote) chain.triggerDropout (m.getFloatVelocity());
        }
        else if (m.isNoteOff())
        {
            if (m.getNoteNumber() == kTapeStopNote) midiTsHeld = false;
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            midiTsHeld = false;

        chain.setTapeStopPressed (tsParam || midiTsHeld.load());
    }
    if (pos < numSamples) chain.process (buffer, pos, numSamples - pos);

    // --- medidor (RMS del bloque, mono-suma) para el VU
    float sumSq = 0.0f, peak = 0.0f;
    const int nch = buffer.getNumChannels();
    for (int c = 0; c < nch; ++c)
    {
        const float* d = buffer.getReadPointer (c);
        for (int i = 0; i < numSamples; ++i) { sumSq += d[i] * d[i]; peak = juce::jmax (peak, std::abs (d[i])); }
    }
    meterRms.store (std::sqrt (sumSq / (float) juce::jmax (1, numSamples * nch)), std::memory_order_relaxed);
    meterPeak.store (juce::jmax (peak, meterPeak.load (std::memory_order_relaxed)), std::memory_order_relaxed);
}

//==============================================================================
//  Dados
//==============================================================================
void LoFiBoxProcessor::rollDie (int index)
{
    if (index == kGlobalDie)
    {
        for (int m = 0; m < kNumModules; ++m)
        {
            if (diceLocks[(size_t) m].load()) continue;
            randomizeModule (apvts, (Module) m, diceRng);
            diceFaces[(size_t) m] = 1 + diceRng.nextInt (6);
            ++diceRollCount[(size_t) m];
        }
    }
    else if (juce::isPositiveAndBelow (index, kNumModules))
        randomizeModule (apvts, (Module) index, diceRng);
    else
        return;

    diceFaces[(size_t) index] = 1 + diceRng.nextInt (6);
    ++diceRollCount[(size_t) index];
}

void LoFiBoxProcessor::toggleDieLock (int m)
{
    if (juce::isPositiveAndBelow (m, kNumModules))
        diceLocks[(size_t) m] = ! diceLocks[(size_t) m].load();
}

//==============================================================================
//  Programas (presets de fábrica) y presets de usuario
//==============================================================================
int LoFiBoxProcessor::getNumPrograms() { return getNumFactoryPresets(); }

void LoFiBoxProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumFactoryPresets())) return;
    currentProgram = index;
    applyFactoryPreset (apvts, index);
    setPresetName (getFactoryPresetName (index));
}

const juce::String LoFiBoxProcessor::getProgramName (int index) { return getFactoryPresetName (index); }

juce::String LoFiBoxProcessor::getPresetName() const
{
    const juce::ScopedLock sl (presetNameLock);
    return presetName;
}

void LoFiBoxProcessor::setPresetName (const juce::String& n)
{
    const juce::ScopedLock sl (presetNameLock);
    presetName = n;
}

juce::ValueTree LoFiBoxProcessor::makeUiStateTree() const
{
    juce::ValueTree ui ("LOFIBOX_UI");
    for (int i = 0; i <= kNumModules; ++i)
        ui.setProperty ("dieFace" + juce::String (i), diceFaces[(size_t) i].load(), nullptr);
    for (int i = 0; i < kNumModules; ++i)
        ui.setProperty ("dieLock" + juce::String (i), diceLocks[(size_t) i].load(), nullptr);
    ui.setProperty ("presetName", getPresetName(), nullptr);
    ui.setProperty ("program", currentProgram, nullptr);
    ui.setProperty ("version", JucePlugin_VersionString, nullptr);
    return ui;
}

void LoFiBoxProcessor::restoreUiStateTree (const juce::ValueTree& ui)
{
    if (! ui.isValid()) return;
    for (int i = 0; i <= kNumModules; ++i)
    {
        diceFaces[(size_t) i] = juce::jlimit (1, 6, (int) ui.getProperty ("dieFace" + juce::String (i), 1 + (i % 6)));
        ++diceRollCount[(size_t) i];   // la GUI anima el cambio de cara
    }
    for (int i = 0; i < kNumModules; ++i)
        diceLocks[(size_t) i] = (bool) ui.getProperty ("dieLock" + juce::String (i), false);
    setPresetName (ui.getProperty ("presetName", "Default").toString());
    currentProgram = (int) ui.getProperty ("program", 0);
}

void LoFiBoxProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.removeChild (state.getChildWithName ("LOFIBOX_UI"), nullptr);
    state.appendChild (makeUiStateTree(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void LoFiBoxProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (tree.hasType (apvts.state.getType()))
        {
            auto ui = tree.getChildWithName ("LOFIBOX_UI").createCopy();
            tree.removeChild (tree.getChildWithName ("LOFIBOX_UI"), nullptr);
            apvts.replaceState (tree);
            restoreUiStateTree (ui);
        }
    }
}

bool LoFiBoxProcessor::saveUserPreset (const juce::File& file)
{
    setPresetName (file.getFileNameWithoutExtension());
    auto state = apvts.copyState();
    state.removeChild (state.getChildWithName ("LOFIBOX_UI"), nullptr);
    state.appendChild (makeUiStateTree(), nullptr);
    if (auto xml = state.createXml())
        return xml->writeTo (file);
    return false;
}

bool LoFiBoxProcessor::loadUserPreset (const juce::File& file)
{
    if (auto xml = juce::XmlDocument::parse (file))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (! tree.hasType (apvts.state.getType())) return false;
        auto ui = tree.getChildWithName ("LOFIBOX_UI").createCopy();
        tree.removeChild (tree.getChildWithName ("LOFIBOX_UI"), nullptr);

        // aplicar parámetro por parámetro notificando al host (automation-safe)
        for (auto* p : getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                auto child = tree.getChildWithProperty ("id", rp->paramID);
                const float real = child.isValid() ? (float) child.getProperty ("value")
                                                   : rp->convertFrom0to1 (rp->getDefaultValue());
                setParamValue (apvts, rp->paramID, real);
            }
        restoreUiStateTree (ui);
        setPresetName (file.getFileNameWithoutExtension());
        return true;
    }
    return false;
}

//==============================================================================
juce::AudioProcessorEditor* LoFiBoxProcessor::createEditor() { return new LoFiBoxEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LoFiBoxProcessor(); }
