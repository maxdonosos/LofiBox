#pragma once
//==============================================================================
//  4. FILTER — HP + LP (SVF TPT 12 dB/oct) para "envejecer" / sonido de radio
//  hp  : 20..2000 Hz
//  lp  : 500..20000 Hz
//  res : 0..1 -> Q 0.707..2.2 (resonancia suave; el HP recibe ~60% de ella)
//  Cutoffs suavizados en el dominio logarítmico (barrido musical).
//==============================================================================
#include "DSPCommon.h"

namespace lofi
{
class FilterModule
{
public:
    struct Params
    {
        bool  enabled = true;
        float hpHz    = 120.0f;
        float lpHz    = 6000.0f;
        float res     = 0.2f;
    };

    void prepare (double sampleRate, int numChannels)
    {
        sr  = (float) sampleRate;
        nch = juce::jmin (numChannels, kMaxChannels);
        logHp.prepare (sampleRate);
        logLp.prepare (sampleRate);
        res.prepare (sampleRate);
        bypass.prepare (sampleRate);
        hp.reset(); lp.reset();
        needsUpdate = true;
    }

    void reset() { hp.reset(); lp.reset(); }

    void setParams (const Params& p) noexcept
    {
        bypass.setActive (p.enabled);
        logHp.set (std::log (juce::jlimit (20.0f, 2000.0f, p.hpHz)));
        logLp.set (std::log (juce::jlimit (500.0f, 20000.0f, p.lpHz)));
        res.set (juce::jlimit (0.0f, 1.0f, p.res));
    }

    void process (juce::AudioBuffer<float>& buffer, int start, int num) noexcept
    {
        const int chans = juce::jmin (buffer.getNumChannels(), nch);
        float* const* ch = buffer.getArrayOfWritePointers();

        if (bypass.isFullyBypassed())
        {
            // seguimos consumiendo los smoothers para no saltar al reactivar
            for (int n = 0; n < num; ++n) { logHp.next(); logLp.next(); res.next(); }
            needsUpdate = true;
            return;
        }

        for (int n = start; n < start + num; ++n)
        {
            const float w = bypass.next();
            if (needsUpdate || logHp.isSmoothing() || logLp.isSmoothing() || res.isSmoothing())
            {
                const float r  = res.next();
                const float qL = 0.707f + 1.5f * r;
                const float qH = 0.707f + 0.9f * r;
                hp.setCoefficients (std::exp (logHp.next()), qH, sr);
                lp.setCoefficients (std::exp (logLp.next()), qL, sr);
                needsUpdate = false;
            }

            for (int c = 0; c < chans; ++c)
            {
                const float x = ch[c][n];
                float y = hp.process (c, x).hp;
                y = lp.process (c, y).lp;
                ch[c][n] = x + w * (y - x);
            }
        }
    }

private:
    float sr = 44100.0f;
    int nch = 2;
    SmoothedParam logHp, logLp, res;
    BypassFade bypass;
    SVF hp, lp;
    bool needsUpdate = true;
};
} // namespace lofi
