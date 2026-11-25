#pragma once
//==============================================================================
//  1. NOISE — hiss de cinta + crackles de vinilo
//  level   : -60..-12 dB (nivel del hiss; los crackles van relativos a este nivel)
//  density : 0..30 crackles/seg (proceso de Poisson: prob. por sample = density/sr)
//  tone    : 0..1 (oscuro 500 Hz -> brillante 14 kHz) filtra hiss y crackles
//==============================================================================
#include "DSPCommon.h"

namespace lofi
{
class NoiseModule
{
public:
    struct Params
    {
        bool  enabled = true;
        float levelDb = -36.0f;
        float density = 4.0f;
        float tone    = 0.5f;
    };

    void prepare (double sampleRate, int numChannels)
    {
        sr = (float) sampleRate;
        nch = juce::jmin (numChannels, kMaxChannels);
        level.prepare (sampleRate);
        tone.prepare (sampleRate);
        bypass.prepare (sampleRate);
        hissLP.reset(); crackleLP.reset(); crackleHP.reset(); dcBlock.reset();
        dcBlock.setCutoff (25.0f, sr);
        crackleHP.setCutoff (180.0f, sr);
        hissWobble.prepare (sr, 0.3f, 1.2f, 1.5f, 0xBADA55u);
        crackleState.fill (0.0f);
        updateFilters (0.5f);
    }

    void setParams (const Params& p) noexcept
    {
        bypass.setActive (p.enabled);
        level.set (dbToGain (p.levelDb));
        tone.set (p.tone);
        density = juce::jlimit (0.0f, 60.0f, p.density);
    }

    void process (juce::AudioBuffer<float>& buffer, int start, int num) noexcept
    {
        if (bypass.isFullyBypassed()) return;

        const int chans = juce::jmin (buffer.getNumChannels(), nch);
        float* const* ch = buffer.getArrayOfWritePointers();
        const float crackleProb = density / sr;

        for (int n = start; n < start + num; ++n)
        {
            const float w = bypass.next();
            const float g = level.next();
            if (tone.isSmoothing()) updateFilters (tone.next());

            // fluctuación lenta del hiss (±1.5 dB), como cinta con oxido irregular
            const float wobble = 1.0f + 0.17f * hissWobble.next();

            // ¿nace un crackle en este sample?
            if (rng.uniform() < crackleProb)
                spawnCrackle (g);

            for (int c = 0; c < chans; ++c)
            {
                // hiss: blanco filtrado (canales decorrelacionados)
                const float hiss = hissLP.process (c, rng.bipolar()).lp * g * wobble * 1.6f;

                // crackle: impulso con decaimiento exponencial, luego HP+LP según tone
                float cr = crackleState[(size_t) c];
                crackleState[(size_t) c] = snapToZero (cr * crackleDecay[(size_t) c]);
                cr = crackleHP.highpass (c, cr);
                cr = crackleLP.process (c, cr).lp;

                const float noise = dcBlock.highpass (c, hiss + cr);
                ch[c][n] += w * noise;
            }
        }
    }

private:
    void spawnCrackle (float hissGain) noexcept
    {
        // mayoría de clicks chicos, de vez en cuando un "pop" grande y más largo
        const float u   = rng.uniform();
        const bool  pop = u > 0.965f;
        const float amp = hissGain * (pop ? 14.0f : (1.5f + 8.0f * u * u * u));
        const float tau = (pop ? 0.0012f : 0.00008f + 0.0004f * rng.uniform()); // segundos
        const float pol = rng.uniform() < 0.5f ? -1.0f : 1.0f;
        const float pan = rng.uniform();                                       // posición estéreo

        for (int c = 0; c < nch; ++c)
        {
            const float panGain = (nch == 1) ? 1.0f : (c == 0 ? std::sqrt (1.0f - pan * 0.7f)
                                                              : std::sqrt (0.3f + pan * 0.7f));
            crackleState[(size_t) c] += pol * amp * panGain;
            crackleDecay[(size_t) c] = std::exp (-1.0f / (tau * sr));
        }
    }

    void updateFilters (float t) noexcept
    {
        const float hissCut = 500.0f * std::pow (28.0f, t);          // 500 Hz .. 14 kHz
        hissLP.setCoefficients (hissCut, 0.6f, sr);
        crackleLP.setCoefficients (juce::jmin (hissCut * 1.4f, 18000.0f), 0.7f, sr);
    }

    float sr = 44100.0f;
    int   nch = 2;
    float density = 0.0f;
    SmoothedParam level, tone;
    BypassFade bypass;
    SVF hissLP, crackleLP;
    OnePole crackleHP, dcBlock;
    SmoothRandom hissWobble;
    std::array<float, kMaxChannels> crackleState {}, crackleDecay { 0.0f, 0.0f };
    FastRandom rng { 0xC0FFEEu };
};
} // namespace lofi
