#pragma once
//==============================================================================
//  5. DROPOUTS — la cinta se traba: volumen y pitch caen JUNTOS
//
//  Cada evento tiene una envolvente e(t) 0..1 con forma natural:
//     ataque (25%) raised-cosine -> meseta (35%) -> release (40%) raised-cosine
//  con mínimo de 12 ms por flanco (muy por sobre la rampa anti-click de 2-5 ms).
//
//  depth (0..1):
//     ganancia mínima  : -1.5 dB (sutil)  ->  -60 dB (corte casi total)
//     déficit de vel.  : ~ -7 cents       ->  ~ -2 semitonos
//  El pitch baja de verdad (lectura de velocidad variable): mientras la cinta
//  va lenta se acumula retraso ("lag"). Al terminar el evento el capstan
//  "recupera" ese retraso con una aceleración raised-cosine acotada a +3%
//  de velocidad (~ +50 cents de pico, normalmente mucho menos).
//
//  Duración aleatoria 50..400 ms (sesgada hacia eventos cortos).
//  Triggers: manual / MIDI (con velocity) y auto (Poisson, eventos/minuto).
//==============================================================================
#include "DSPCommon.h"

namespace lofi
{
class DropoutModule
{
public:
    enum class Mode { Manual = 0, Auto = 1 };

    struct Params
    {
        bool  enabled = true;
        float depth   = 0.5f;   // 0..1
        Mode  mode    = Mode::Manual;
        float ratePerMinute = 6.0f; // 0..60 (solo en Auto)
    };

    void prepare (double sampleRate, int numChannels)
    {
        sr  = (float) sampleRate;
        nch = juce::jmin (numChannels, kMaxChannels);
        delay.prepare (nch, (int) (sr * 0.5f));
        maxLag = sr * 0.45f;
        depth.prepare (sampleRate);
        bypass.prepare (sampleRate);
        resetEvent();
    }

    void reset() { delay.reset(); resetEvent(); }

    void setParams (const Params& p) noexcept
    {
        bypass.setActive (p.enabled);
        depth.set (juce::jlimit (0.0f, 1.0f, p.depth));
        mode = p.mode;
        rate = juce::jlimit (0.0f, 120.0f, p.ratePerMinute);
    }

    /** Dispara un dropout (desde botón, automatización o nota MIDI). velocity 0..1 */
    void trigger (float velocity = 1.0f) noexcept
    {
        if (bypass.isFullyBypassed()) return;
        startEvent (0.5f + 0.5f * juce::jlimit (0.0f, 1.0f, velocity));
    }

    /** Nivel de envolvente actual (para un LED en la GUI). */
    float getActivity() const noexcept { return activity.load (std::memory_order_relaxed); }

    void process (juce::AudioBuffer<float>& buffer, int start, int num) noexcept
    {
        if (bypass.isFullyBypassed())
        {
            if (active || lag > 0.0f) resetEvent();
            for (int n = 0; n < num; ++n) depth.next();
            activity.store (0.0f, std::memory_order_relaxed);
            return;
        }

        const int chans = juce::jmin (buffer.getNumChannels(), nch);
        float* const* ch = buffer.getArrayOfWritePointers();
        const float autoProb = (mode == Mode::Auto) ? rate / (60.0f * sr) : 0.0f;

        for (int n = start; n < start + num; ++n)
        {
            const float w = bypass.next();
            const float d = depth.next();

            if (autoProb > 0.0f && ! active && rng.uniform() < autoProb)
                startEvent (1.0f);

            // --- envolvente del evento
            float e = 0.0f;
            if (active)
            {
                e = computeEnvelope();
                if (++pos >= total) { active = false; e = 0.0f; beginPayback(); }
            }
            lastEnv = e;

            // --- velocidad: déficit durante el evento, exceso durante la recuperación
            const float dd = juce::jlimit (0.0f, 1.0f, d * eventScale);
            float speedDelta = -e * (0.004f + 0.106f * dd);
            if (paybackLen > 0)
            {
                const float t = (float) paybackPos / (float) paybackLen;
                speedDelta += paybackAmount / (float) paybackLen * (1.0f - std::cos (kTwoPi * t));
                if (++paybackPos >= paybackLen) { paybackLen = 0; lag = 0.0f; speedDelta = 0.0f; }
            }
            lag = juce::jlimit (0.0f, maxLag, lag - speedDelta);

            // --- ganancia (en dB, interpolada por la envolvente)
            const float floorDb = -1.5f - 58.5f * std::pow (dd, 1.4f);
            const float gain = dbToGain (e * floorDb);

            for (int c = 0; c < chans; ++c)
            {
                const float x = ch[c][n];
                delay.write (c, x);
                const float y = (lag > 0.0f ? delay.read (c, lag) : x) * gain;
                ch[c][n] = x + w * (y - x);
            }
            delay.advance();
        }
        activity.store (lastEnv, std::memory_order_relaxed);
    }

private:
    void resetEvent() noexcept
    {
        active = false; pos = total = 0; lag = 0.0f;
        paybackLen = paybackPos = 0; paybackAmount = 0.0f; lastEnv = 0.0f;
    }

    void startEvent (float scale) noexcept
    {
        // retrigger suave: el ataque nuevo arranca desde el nivel actual
        envStart = active ? lastEnv : 0.0f;
        const float u = rng.uniform();
        const float durSec = 0.050f + 0.350f * u * std::sqrt (u);    // 50..400 ms
        total   = juce::jmax (1, (int) (durSec * sr));
        attack  = juce::jmax ((int) (0.012f * sr), (int) (0.25f * (float) total));
        release = juce::jmax ((int) (0.012f * sr), (int) (0.40f * (float) total));
        total   = juce::jmax (total, attack + release + 1);
        pos     = 0;
        eventScale = scale * (0.8f + 0.4f * rng.uniform());          // variación por evento
        active  = true;
        paybackLen = 0; paybackPos = 0;   // si estaba recuperando, el lag acumulado sigue
    }

    float computeEnvelope() const noexcept
    {
        if (pos < attack)            return envStart + (1.0f - envStart) * raisedCos ((float) pos / (float) attack);
        if (pos < total - release)   return 1.0f;
        return 1.0f - raisedCos ((float) (pos - (total - release)) / (float) release);
    }

    void beginPayback() noexcept
    {
        if (lag <= 0.0f) return;
        paybackAmount = lag;
        const float maxExcess = 0.03f;                         // +3% vel. de pico
        paybackLen = juce::jmax ((int) (0.6f * sr), (int) (2.0f * lag / maxExcess));
        paybackPos = 0;
    }

    float sr = 44100.0f;
    int nch = 2;
    DelayLine delay;
    SmoothedParam depth;
    BypassFade bypass;
    Mode mode = Mode::Manual;
    float rate = 0.0f;
    FastRandom rng { 0xD509u };

    bool active = false;
    int pos = 0, total = 0, attack = 1, release = 1;
    float envStart = 0.0f, eventScale = 1.0f, lastEnv = 0.0f;
    float lag = 0.0f, maxLag = 1.0f, paybackAmount = 0.0f;
    int paybackLen = 0, paybackPos = 0;
    std::atomic<float> activity { 0.0f };
};
} // namespace lofi
