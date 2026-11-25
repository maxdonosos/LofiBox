#pragma once
//==============================================================================
//  6. TAPESTOP — lectura de buffer a velocidad variable (NO pitch-shifter)
//
//  Se escribe siempre a un buffer circular. Un cabezal de lectura avanza a
//  velocidad v (1 = normal). Al frenar, v cae de 1 a 0 siguiendo la curva:
//  pitch y velocidad bajan juntos, exactamente como una cinta/tornamesa.
//
//  Estado interno: u ∈ [0,1] avanza linealmente en el tiempo (1/T por sample)
//  y la velocidad es v = curva(u). Frenar = u baja; arrancar = u sube con la
//  MISMA curva (espejo temporal). Invertir a mitad de camino es continuo.
//
//  Anti-click:
//   - fade de 4 ms al llegar a v=0 (y al salir de v=0 en el arranque)
//   - al terminar el arranque la lectura quedó atrasada respecto al audio en
//     vivo: crossfade equal-power de 12 ms de vuelta al "live".
//   - DC-blocker de 5 Hz en la salida (el último sample congelado no deja DC).
//
//  Modos de reinicio:
//   - Release : momentáneo. Mantener apretado = frena; soltar = arranca.
//   - Retrigger: toggle. Un golpe frena (y queda detenido), otro golpe arranca.
//==============================================================================
#include "DSPCommon.h"

namespace lofi
{
class TapeStopModule
{
public:
    enum class Curve   { Linear = 0, Exponential = 1, SCurve = 2 };
    enum class Restart { OnRelease = 0, OnRetrigger = 1 };
    enum class State   { Running = 0, Stopping, Stopped, Starting, CatchUp };

    struct Params
    {
        bool    enabled  = true;
        float   timeSec  = 0.5f;          // tiempo de frenado (ya resuelto sync/free)
        Curve   curve    = Curve::Exponential;
        Restart restart  = Restart::OnRelease;
    };

    static constexpr float kMaxTimeSec = 8.0f;

    void prepare (double sampleRate, int numChannels)
    {
        sr  = (float) sampleRate;
        nch = juce::jmin (numChannels, kMaxChannels);
        buffer.prepare (nch, (int) (sr * (2.0f * kMaxTimeSec + 1.0f)));
        fadeLen     = 0.004f * sr;
        catchUpLen  = juce::jmax (1, (int) (0.012f * sr));
        dcBlock.setCutoff (5.0f, sr);
        dcBlock.reset();
        bypass.prepare (sampleRate);
        forceRunning();
    }

    void reset() { buffer.reset(); dcBlock.reset(); forceRunning(); }

    void setParams (const Params& p) noexcept
    {
        bypass.setActive (p.enabled);
        timeSamples = juce::jlimit (0.01f, kMaxTimeSec, p.timeSec) * sr;
        curve = p.curve;
        restart = p.restart;

        // En modo momentáneo, si el botón no está apretado no puede quedarse detenido
        if (restart == Restart::OnRelease && ! pressed
            && (state == State::Stopping || state == State::Stopped))
            beginStart();
    }

    /** Estado del botón (GUI/automatización/MIDI combinados). Llamar sample-accurate. */
    void setPressed (bool isDown) noexcept
    {
        if (isDown == pressed) return;
        pressed = isDown;
        if (bypass.isFullyBypassed()) return;

        if (restart == Restart::OnRelease)
        {
            if (isDown) requestStop();
            else        beginStart();
        }
        else if (isDown) // toggle en cada flanco de subida
        {
            if (state == State::Stopping || state == State::Stopped) beginStart();
            else                                                    requestStop();
        }
    }

    float getSpeed() const noexcept  { return displaySpeed.load (std::memory_order_relaxed); }
    State getState() const noexcept  { return (State) displayState.load (std::memory_order_relaxed); }

    void process (juce::AudioBuffer<float>& buf, int start, int num) noexcept
    {
        const int chans = juce::jmin (buf.getNumChannels(), nch);
        float* const* ch = buf.getArrayOfWritePointers();

        for (int n = start; n < start + num; ++n)
        {
            const float w = bypass.next();
            if (w <= 0.0f && bypass.isFullyBypassed() && state != State::Running)
                forceRunning();

            const float inc = 1.0f / juce::jmax (1.0f, timeSamples);

            switch (state)
            {
                case State::Running: break;
                case State::Stopping:
                    u -= inc;
                    if (u <= 0.0f) { u = 0.0f; state = State::Stopped; }
                    break;
                case State::Stopped: break;
                case State::Starting:
                    u += inc;
                    if (u >= 1.0f)
                    {
                        u = 1.0f;
                        if (lag > 0.5f) { state = State::CatchUp; catchUpPos = 0; }
                        else            { state = State::Running; lag = 0.0f; }
                    }
                    break;
                case State::CatchUp:
                    if (++catchUpPos >= catchUpLen)
                    {
                        state = State::Running; lag = 0.0f;
                        if (pendingStop) { pendingStop = false; state = State::Stopping; }
                    }
                    break;
            }

            const float v = (state == State::Running || state == State::CatchUp) ? 1.0f : shape (u);
            if (state == State::Stopping || state == State::Starting)
                lag = juce::jmin (lag + (1.0f - v), (float) buffer.getMaxDelay() - 4.0f);

            // ganancia anti-click alrededor de v = 0
            float g = 1.0f;
            if (state == State::Stopped) g = 0.0f;
            else if (state == State::Stopping || state == State::Starting)
            {
                const float k = juce::jlimit (0.0f, 1.0f, (u * timeSamples) / fadeLen);
                g = k * k * (3.0f - 2.0f * k);
            }

            // crossfade de catch-up (equal power)
            float xLag = 1.0f, xLive = 0.0f;
            if (state == State::CatchUp)
            {
                const float t = (float) catchUpPos / (float) catchUpLen;
                xLag  = std::cos (0.5f * kPi * t);
                xLive = std::sin (0.5f * kPi * t);
            }

            for (int c = 0; c < chans; ++c)
            {
                const float x = ch[c][n];
                buffer.write (c, x);
                float y;
                if (state == State::Running)       y = x;
                else if (state == State::Stopped)  y = 0.0f;
                else                               y = buffer.read (c, lag) * g;
                if (state == State::CatchUp)       y = y * xLag + x * xLive;

                y = dcBlock.highpass (c, y);
                ch[c][n] = x + w * (y - x);
            }
            buffer.advance();
            lastSpeed = (state == State::Stopped) ? 0.0f : v;
        }
        displaySpeed.store (lastSpeed, std::memory_order_relaxed);
        displayState.store ((int) state, std::memory_order_relaxed);
    }

private:
    float shape (float x) const noexcept
    {
        switch (curve)
        {
            case Curve::Linear:      return x;
            case Curve::Exponential: return (std::exp (3.5f * x) - 1.0f) / (std::exp (3.5f) - 1.0f);
            case Curve::SCurve:      return x * x * (3.0f - 2.0f * x);
        }
        return x;
    }

    void requestStop() noexcept
    {
        if (state == State::CatchUp) { pendingStop = true; return; }  // 12 ms después
        if (state == State::Running) { lag = 0.0f; u = 1.0f; }
        if (state != State::Stopped) state = State::Stopping;
    }

    void beginStart() noexcept
    {
        pendingStop = false;
        if (state == State::Stopped)
        {
            // la ganancia está en 0: arrancamos desde el audio en vivo
            lag = 0.0f; u = 0.0f;
            state = State::Starting;
        }
        else if (state == State::Stopping)
            state = State::Starting;                  // inversión continua
    }

    void forceRunning() noexcept
    {
        state = State::Running; u = 1.0f; lag = 0.0f; pendingStop = false; catchUpPos = 0;
    }

    float sr = 44100.0f;
    int nch = 2;
    DelayLine buffer;
    OnePole dcBlock;
    BypassFade bypass;
    Curve curve = Curve::Exponential;
    Restart restart = Restart::OnRelease;
    State state = State::Running;
    bool pressed = false, pendingStop = false;
    float u = 1.0f, lag = 0.0f, timeSamples = 22050.0f, fadeLen = 176.0f, lastSpeed = 1.0f;
    int catchUpLen = 529, catchUpPos = 0;
    std::atomic<float> displaySpeed { 1.0f };
    std::atomic<int>   displayState { 0 };
};
} // namespace lofi
