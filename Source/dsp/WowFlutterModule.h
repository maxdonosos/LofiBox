#pragma once
//==============================================================================
//  2. WOW & FLUTTER — modulación de pitch tipo cinta vía delay modulado
//
//  Un delay d(t) = base + A·sin(2πft) produce una desviación de velocidad
//  máxima de 2πfA. Por eso la profundidad se expresa en CENTS y la amplitud
//  se calcula como A = (2^(c/1200) - 1) / (2πf), acotada por el presupuesto
//  de delay disponible. Así "depth" suena igual a cualquier rate.
//
//  Latencia: el delay base es FIJO (24 ms) y se reporta al host. En bypass se
//  lee el mismo delay base sin modular, así la latencia nunca cambia.
//
//  drift: ruido de control suave que (a) desplaza el delay lentamente,
//  (b) hace variar el rate del wow y (c) la profundidad del flutter,
//  como un motor de cassette viejo que no mantiene la velocidad.
//==============================================================================
#include "DSPCommon.h"

namespace lofi
{
class WowFlutterModule
{
public:
    struct Params
    {
        bool  enabled       = true;
        float wowRate       = 0.6f;   // Hz 0.1..2
        float wowDepth      = 12.0f;  // cents 0..50
        float flutterRate   = 6.0f;   // Hz 2..10
        float flutterDepth  = 3.0f;   // cents 0..15
        float drift         = 0.3f;   // 0..1
    };

    static constexpr float kBaseDelaySec    = 0.024f;
    static constexpr float kWowMaxSec       = 0.0180f;
    static constexpr float kFlutterMaxSec   = 0.0015f;
    static constexpr float kDriftMaxSec     = 0.0040f;

    void prepare (double sampleRate, int numChannels)
    {
        sr  = (float) sampleRate;
        nch = juce::jmin (numChannels, kMaxChannels);
        baseDelay = (float) juce::roundToInt (kBaseDelaySec * sr);
        delay.prepare (nch, (int) (sr * 0.05f));
        for (auto* p : { &wowRate, &wowDepth, &flutterRate, &flutterDepth, &drift })
            p->prepare (sampleRate);
        bypass.prepare (sampleRate);
        driftDelay.prepare (sr, 0.8f, 3.0f, 0.35f, 0x1234567u);
        driftRate .prepare (sr, 1.0f, 4.0f, 0.25f, 0x7654321u);
        driftFlut .prepare (sr, 0.2f, 0.9f, 1.20f, 0x0F0F0F0u);
        wowPhase = flutPhase = flut2Phase = 0.0f;
        prevMod = 0.0f;
        currentSpeed.store (1.0f);
    }

    void reset() { delay.reset(); }

    int getLatencySamples() const noexcept { return (int) baseDelay; }

    /** Velocidad instantánea de la "cinta" (para animar los carretes). */
    float getCurrentSpeed() const noexcept { return currentSpeed.load (std::memory_order_relaxed); }

    void setParams (const Params& p) noexcept
    {
        bypass.setActive (p.enabled);
        wowRate.set      (juce::jlimit (0.05f, 4.0f, p.wowRate));
        wowDepth.set     (juce::jlimit (0.0f, 60.0f, p.wowDepth));
        flutterRate.set  (juce::jlimit (1.0f, 15.0f, p.flutterRate));
        flutterDepth.set (juce::jlimit (0.0f, 20.0f, p.flutterDepth));
        drift.set        (juce::jlimit (0.0f, 1.0f, p.drift));
    }

    void process (juce::AudioBuffer<float>& buffer, int start, int num) noexcept
    {
        const int chans = juce::jmin (buffer.getNumChannels(), nch);
        float* const* ch = buffer.getArrayOfWritePointers();
        const float invSr = 1.0f / sr;
        float lastSpeed = 1.0f;

        for (int n = start; n < start + num; ++n)
        {
            const float w  = bypass.next();
            const float dr = drift.next();

            // --- ruido de drift
            const float rDelay = driftDelay.next();
            const float rRate  = driftRate.next();
            const float rFlut  = driftFlut.next();

            // --- WOW
            const float wRate = wowRate.next() * (1.0f + 0.35f * dr * rRate);
            const float wDev  = centsToRatio (wowDepth.next()) - 1.0f;
            const float wAmp  = juce::jmin (wDev / (kTwoPi * wRate), kWowMaxSec);
            wowPhase += wRate * invSr;
            if (wowPhase >= 1.0f) wowPhase -= 1.0f;
            // sinusoide levemente asimétrica (cabrestante excéntrico)
            const float wowLfo = std::sin (kTwoPi * wowPhase) + 0.12f * std::sin (2.0f * kTwoPi * wowPhase + 0.7f);

            // --- FLUTTER (fundamental + parcial inarmónico)
            const float fRate = flutterRate.next();
            const float fDev  = (centsToRatio (flutterDepth.next()) - 1.0f) * (1.0f + 0.5f * dr * rFlut);
            const float fAmp  = juce::jmin (fDev / (kTwoPi * fRate), kFlutterMaxSec);
            flutPhase  += fRate * invSr;          if (flutPhase  >= 1.0f) flutPhase  -= 1.0f;
            flut2Phase += fRate * 1.73f * invSr;  if (flut2Phase >= 1.0f) flut2Phase -= 1.0f;
            const float flutLfo = (std::sin (kTwoPi * flutPhase) + 0.3f * std::sin (kTwoPi * flut2Phase)) / 1.3f;

            // --- DRIFT del delay (lento)
            const float dDelay = dr * kDriftMaxSec * rDelay;

            const float modSec = (wAmp * wowLfo / 1.12f) + fAmp * flutLfo + dDelay;
            const float mod    = modSec * sr * w;               // bypass = sin modulación

            lastSpeed = 1.0f - (mod - prevMod);
            prevMod = mod;

            for (int c = 0; c < chans; ++c)
            {
                delay.write (c, ch[c][n]);
                ch[c][n] = delay.read (c, baseDelay + mod);
            }
            delay.advance();
        }
        currentSpeed.store (lastSpeed, std::memory_order_relaxed);
    }

private:
    float sr = 44100.0f, baseDelay = 1000.0f;
    int nch = 2;
    DelayLine delay;
    SmoothedParam wowRate, wowDepth, flutterRate, flutterDepth, drift;
    BypassFade bypass;
    SmoothRandom driftDelay, driftRate, driftFlut;
    float wowPhase = 0.0f, flutPhase = 0.0f, flut2Phase = 0.0f, prevMod = 0.0f;
    std::atomic<float> currentSpeed { 1.0f };
};
} // namespace lofi
