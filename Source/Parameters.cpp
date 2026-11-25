#include "Parameters.h"

namespace lofi
{
using APF  = juce::AudioParameterFloat;
using APB  = juce::AudioParameterBool;
using APC  = juce::AudioParameterChoice;
using Attr = juce::AudioParameterFloatAttributes;

namespace
{
    juce::NormalisableRange<float> range (float lo, float hi, float centre = -1.0f, float step = 0.0f)
    {
        juce::NormalisableRange<float> r (lo, hi, step);
        if (centre > lo && centre < hi) r.setSkewForCentre (centre);
        return r;
    }

    juce::String fmtHz (float v)
    {
        if (v >= 1000.0f) return juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz";
        if (v < 10.0f)    return juce::String (v, 2) + " Hz";
        return juce::String (juce::roundToInt (v)) + " Hz";
    }

    Attr attr (std::function<juce::String (float)> f, const juce::String& label = {})
    {
        return Attr().withStringFromValueFunction ([f] (float v, int) { return f (v); }).withLabel (label);
    }

    auto dB      = [] (float v) { return juce::String (v, 1) + " dB"; };
    auto pct     = [] (float v) { return juce::String (juce::roundToInt (v)) + "%"; };
    auto cents   = [] (float v) { return juce::String (v, 1) + " ct"; };
    auto hz      = [] (float v) { return fmtHz (v); };
    auto bits    = [] (float v) { return juce::String (v, 1) + " bit"; };
    auto perSec  = [] (float v) { return juce::String (v, 1) + " /s"; };
    auto perMin  = [] (float v) { return juce::String (v, 1) + " /min"; };
    auto ms      = [] (float v) { return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " s" : juce::String (juce::roundToInt (v)) + " ms"; };
    auto rateFmt = [] (float v) { return v >= 47999.0f ? juce::String ("OFF") : fmtHz (v); };
}

const juce::StringArray& tapeStopDivisionNames()
{
    static const juce::StringArray names { "1/16", "1/8", "1/4", "1/2", "1 BAR", "2 BARS" };
    return names;
}

double tapeStopDivisionBeats (int index, int num, int den)
{
    const double barBeats = (double) juce::jmax (1, num) * 4.0 / (double) juce::jmax (1, den);
    switch (index)
    {
        case 0: return 0.25;
        case 1: return 0.5;
        case 2: return 1.0;
        case 3: return 2.0;
        case 4: return barBeats;
        case 5: return barBeats * 2.0;
        default: return 1.0;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout L;
    auto pid = [] (const char* id) { return juce::ParameterID { id, 1 }; };

    // ---- NOISE
    auto noise = std::make_unique<juce::AudioProcessorParameterGroup> ("noise", "Noise", "|");
    noise->addChild (std::make_unique<APB> (pid (ID::noiseOn), "Noise On", true));
    noise->addChild (std::make_unique<APF> (pid (ID::noiseLevel),   "Noise Level",   range (-60.0f, -12.0f), -40.0f, attr (dB, "dB")));
    noise->addChild (std::make_unique<APF> (pid (ID::noiseDensity), "Crackle Density", range (0.0f, 30.0f, 5.0f), 3.0f, attr (perSec, "/s")));
    noise->addChild (std::make_unique<APF> (pid (ID::noiseTone),    "Noise Tone",    range (0.0f, 100.0f), 45.0f, attr (pct, "%")));
    L.add (std::move (noise));

    // ---- WOW & FLUTTER
    auto wow = std::make_unique<juce::AudioProcessorParameterGroup> ("wow", "Wow & Flutter", "|");
    wow->addChild (std::make_unique<APB> (pid (ID::wowOn), "Wow On", true));
    wow->addChild (std::make_unique<APF> (pid (ID::wowRate),      "Wow Rate",      range (0.1f, 2.0f, 0.6f), 0.55f, attr (hz, "Hz")));
    wow->addChild (std::make_unique<APF> (pid (ID::wowDepth),     "Wow Depth",     range (0.0f, 50.0f, 15.0f), 10.0f, attr (cents, "ct")));
    wow->addChild (std::make_unique<APF> (pid (ID::flutterRate),  "Flutter Rate",  range (2.0f, 10.0f, 5.5f), 6.0f, attr (hz, "Hz")));
    wow->addChild (std::make_unique<APF> (pid (ID::flutterDepth), "Flutter Depth", range (0.0f, 15.0f, 4.0f), 2.5f, attr (cents, "ct")));
    wow->addChild (std::make_unique<APF> (pid (ID::wowDrift),     "Drift",         range (0.0f, 100.0f), 25.0f, attr (pct, "%")));
    L.add (std::move (wow));

    // ---- CRUSH
    auto crush = std::make_unique<juce::AudioProcessorParameterGroup> ("crush", "Crush", "|");
    crush->addChild (std::make_unique<APB> (pid (ID::crushOn), "Crush On", true));
    crush->addChild (std::make_unique<APF> (pid (ID::crushBits), "Bit Depth",   range (4.0f, 16.0f), 12.0f, attr (bits, "bit")));
    crush->addChild (std::make_unique<APF> (pid (ID::crushRate), "Sample Rate", range (2000.0f, 48000.0f, 11000.0f), 18000.0f, attr (rateFmt, "Hz")));
    crush->addChild (std::make_unique<APF> (pid (ID::crushMix),  "Crush Mix",   range (0.0f, 100.0f), 40.0f, attr (pct, "%")));
    L.add (std::move (crush));

    // ---- FILTER
    auto filter = std::make_unique<juce::AudioProcessorParameterGroup> ("filter", "Filter", "|");
    filter->addChild (std::make_unique<APB> (pid (ID::filterOn), "Filter On", true));
    filter->addChild (std::make_unique<APF> (pid (ID::filterHp),  "High-pass", range (20.0f, 2000.0f, 200.0f), 60.0f, attr (hz, "Hz")));
    filter->addChild (std::make_unique<APF> (pid (ID::filterLp),  "Low-pass",  range (500.0f, 20000.0f, 4000.0f), 9000.0f, attr (hz, "Hz")));
    filter->addChild (std::make_unique<APF> (pid (ID::filterRes), "Resonance", range (0.0f, 100.0f), 15.0f, attr (pct, "%")));
    L.add (std::move (filter));

    // ---- DROPOUTS
    auto drop = std::make_unique<juce::AudioProcessorParameterGroup> ("dropouts", "Dropouts", "|");
    drop->addChild (std::make_unique<APB> (pid (ID::dropOn), "Dropouts On", true));
    drop->addChild (std::make_unique<APF> (pid (ID::dropDepth), "Dropout Depth", range (0.0f, 100.0f), 45.0f, attr (pct, "%")));
    drop->addChild (std::make_unique<APC> (pid (ID::dropMode),  "Dropout Mode", juce::StringArray { "Manual", "Auto" }, 0));
    drop->addChild (std::make_unique<APF> (pid (ID::dropRate),  "Dropout Rate", range (0.0f, 60.0f, 10.0f), 6.0f, attr (perMin, "/min")));
    drop->addChild (std::make_unique<APB> (pid (ID::dropTrigger), "Dropout Trigger", false));
    L.add (std::move (drop));

    // ---- TAPESTOP
    auto ts = std::make_unique<juce::AudioProcessorParameterGroup> ("tapestop", "Tapestop", "|");
    ts->addChild (std::make_unique<APB> (pid (ID::tsOn), "Tapestop On", true));
    ts->addChild (std::make_unique<APB> (pid (ID::tsTrigger), "Tapestop Trigger", false));
    ts->addChild (std::make_unique<APB> (pid (ID::tsSync), "Tapestop Sync", true));
    ts->addChild (std::make_unique<APC> (pid (ID::tsDivision), "Tapestop Division", tapeStopDivisionNames(), 3));
    ts->addChild (std::make_unique<APF> (pid (ID::tsTime), "Tapestop Time", range (30.0f, 4000.0f, 600.0f), 700.0f, attr (ms, "ms")));
    ts->addChild (std::make_unique<APC> (pid (ID::tsCurve), "Tapestop Curve", juce::StringArray { "Linear", "Exponential", "S-Curve" }, 1));
    ts->addChild (std::make_unique<APC> (pid (ID::tsRestart), "Tapestop Restart", juce::StringArray { "On Release", "On Retrigger" }, 0));
    L.add (std::move (ts));

    // ---- MASTER
    auto master = std::make_unique<juce::AudioProcessorParameterGroup> ("master", "Master", "|");
    master->addChild (std::make_unique<APF> (pid (ID::mix),    "Mix",    range (0.0f, 100.0f), 100.0f, attr (pct, "%")));
    master->addChild (std::make_unique<APF> (pid (ID::output), "Output", range (-24.0f, 12.0f), 0.0f, attr (dB, "dB")));
    L.add (std::move (master));

    return L;
}

void setParamValue (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float realValue)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
        p->endChangeGesture();
    }
}

//==============================================================================
//  RANDOMIZERS — rangos elegidos para que SIEMPRE suene musical:
//  nada de bit depth < 6, sample rate < 4 kHz, LP < 2.5 kHz, wow > 35 ct, etc.
//==============================================================================
namespace
{
    float uni (juce::Random& r, float lo, float hi)    { return lo + (hi - lo) * r.nextFloat(); }
    float logUni (juce::Random& r, float lo, float hi) { return std::exp (uni (r, std::log (lo), std::log (hi))); }
    // sesgo hacia valores bajos (más sutil más seguido)
    float lowBiased (juce::Random& r, float lo, float hi) { const float u = r.nextFloat(); return lo + (hi - lo) * u * u; }
}

void randomizeModule (juce::AudioProcessorValueTreeState& apvts, Module m, juce::Random& rng)
{
    auto set = [&apvts] (const char* id, float v) { setParamValue (apvts, id, v); };

    switch (m)
    {
        case Module::Noise:
            set (ID::noiseLevel,   uni (rng, -50.0f, -28.0f));
            set (ID::noiseDensity, lowBiased (rng, 0.3f, 12.0f));
            set (ID::noiseTone,    uni (rng, 20.0f, 75.0f));
            break;

        case Module::Wow:
            set (ID::wowRate,      logUni (rng, 0.25f, 1.3f));
            set (ID::wowDepth,     lowBiased (rng, 4.0f, 32.0f));
            set (ID::flutterRate,  uni (rng, 4.0f, 9.0f));
            set (ID::flutterDepth, lowBiased (rng, 0.5f, 7.0f));
            set (ID::wowDrift,     uni (rng, 5.0f, 70.0f));
            break;

        case Module::Crush:
            set (ID::crushBits, uni (rng, 6.0f, 13.0f));
            set (ID::crushRate, logUni (rng, 4500.0f, 24000.0f));
            set (ID::crushMix,  uni (rng, 25.0f, 80.0f));
            break;

        case Module::Filter:
        {
            const float hp = logUni (rng, 30.0f, 320.0f);
            const float lp = logUni (rng, juce::jmax (2500.0f, hp * 12.0f), 14000.0f);
            set (ID::filterHp,  hp);
            set (ID::filterLp,  lp);
            set (ID::filterRes, uni (rng, 0.0f, 50.0f));
            break;
        }

        case Module::Dropout:
            set (ID::dropDepth, uni (rng, 15.0f, 75.0f));
            set (ID::dropRate,  lowBiased (rng, 1.0f, 15.0f));
            break;

        case Module::TapeStop:
        {
            const bool sync = apvts.getRawParameterValue (ID::tsSync)->load() > 0.5f;
            if (sync) set (ID::tsDivision, (float) (1 + rng.nextInt (4)));   // 1/8 .. 1 BAR
            else      set (ID::tsTime, logUni (rng, 150.0f, 1500.0f));
            set (ID::tsCurve, (float) rng.nextInt (3));
            break;
        }

        case Module::NumModules: break;
    }
}
} // namespace lofi
