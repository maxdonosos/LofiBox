#pragma once
//==============================================================================
//  Lo-Fi Box — utilidades DSP compartidas
//  - RNG realtime-safe (xorshift, sin locks ni allocations)
//  - Smoothed values (~20 ms)
//  - Delay line circular con lectura fraccional (Hermite 4 puntos)
//  - SVF TPT (Cytomic/Zavalishin) con protección anti-denormals
//  - One-pole LP/HP
//==============================================================================
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <cstdint>
#include <vector>
#include <array>

namespace lofi
{
constexpr float kSmoothSeconds = 0.020f;   // suavizado de parámetros (anti zipper)
constexpr float kBypassSeconds = 0.010f;   // crossfade de bypass por módulo
constexpr int   kMaxChannels   = 2;
constexpr float kPi    = juce::MathConstants<float>::pi;
constexpr float kTwoPi = juce::MathConstants<float>::twoPi;

/** Anti-denormal: aplasta a cero valores minúsculos en estados de filtros/feedback. */
inline float snapToZero (float x) noexcept { return (x > -1.0e-15f && x < 1.0e-15f) ? 0.0f : x; }

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float centsToRatio (float cents) noexcept { return std::exp2 (cents / 1200.0f); }

/** Raised-cosine 0..1 (curva "natural", derivada cero en los extremos). */
inline float raisedCos (float t) noexcept
{
    t = juce::jlimit (0.0f, 1.0f, t);
    return 0.5f - 0.5f * std::cos (kPi * t);
}

//==============================================================================
/** xorshift32: rápido, determinista y seguro en el audio thread. */
class FastRandom
{
public:
    explicit FastRandom (uint32_t seed = 0x9E3779B9u) noexcept { setSeed (seed); }
    void setSeed (uint32_t s) noexcept { state = (s != 0u ? s : 0x12345u); }

    uint32_t next() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }
    float uniform() noexcept { return (float) (next() >> 8) * (1.0f / 16777216.0f); } // [0,1)
    float bipolar() noexcept { return uniform() * 2.0f - 1.0f; }                       // [-1,1)

private:
    uint32_t state = 1u;
};

//==============================================================================
using Smoothed = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>;

/** Helper: en el primer set salta directo al valor (sin rampa desde 0 al cargar). */
struct SmoothedParam
{
    Smoothed value;
    bool initialised = false;

    void prepare (double sampleRate, float seconds = kSmoothSeconds)
    {
        value.reset (sampleRate, seconds);
        initialised = false;
    }
    void set (float target) noexcept
    {
        if (! initialised) { value.setCurrentAndTargetValue (target); initialised = true; }
        else               value.setTargetValue (target);
    }
    float next() noexcept               { return value.getNextValue(); }
    float current() const noexcept      { return value.getCurrentValue(); }
    float target() const noexcept       { return value.getTargetValue(); }
    bool  isSmoothing() const noexcept  { return value.isSmoothing(); }
};

//==============================================================================
/** Delay line circular multicanal con lectura fraccional.
    Contrato por sample: write(ch, x) en todos los canales -> read(ch, d) -> advance().
    d = 0 devuelve el sample recién escrito. */
class DelayLine
{
public:
    void prepare (int numChannels, int maxDelaySamples)
    {
        size = 1;
        while (size < maxDelaySamples + 8) size <<= 1;
        mask = size - 1;
        data.assign ((size_t) numChannels, std::vector<float> ((size_t) size, 0.0f));
        writePos = 0;
    }
    void reset() { for (auto& c : data) std::fill (c.begin(), c.end(), 0.0f); writePos = 0; }

    int  getMaxDelay() const noexcept         { return size - 8; }
    void write (int ch, float x) noexcept     { data[(size_t) ch][(size_t) writePos] = x; }
    void advance() noexcept                   { writePos = (writePos + 1) & mask; }

    float read (int ch, float delay) const noexcept
    {
        delay = juce::jlimit (0.0f, (float) getMaxDelay(), delay);
        const int   i = (int) delay;
        const float f = delay - (float) i;
        const float* b = data[(size_t) ch].data();
        const int p = writePos - i;
        auto at = [b, p, this] (int k) noexcept { return b[(p - k) & mask]; };

        if (i == 0)  // no hay "futuro" disponible: lineal entre x[n] y x[n-1]
            return at (0) + f * (at (1) - at (0));

        // Hermite 4 puntos entre y0 (= x[n-i]) e y1 (= x[n-i-1])
        const float ym1 = at (-1), y0 = at (0), y1 = at (1), y2 = at (2);
        const float c1 = 0.5f * (y1 - ym1);
        const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
        const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
        return ((c3 * f + c2) * f + c1) * f + y0;
    }

private:
    std::vector<std::vector<float>> data;
    int size = 0, mask = 0, writePos = 0;
};

//==============================================================================
/** State Variable Filter TPT (topology-preserving transform), 12 dB/oct. */
class SVF
{
public:
    void setCoefficients (float cutoffHz, float q, float sampleRate) noexcept
    {
        cutoffHz = juce::jlimit (5.0f, sampleRate * 0.49f, cutoffHz);
        g  = std::tan (kPi * cutoffHz / sampleRate);
        k  = 1.0f / juce::jmax (0.1f, q);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void reset() noexcept { ic1.fill (0.0f); ic2.fill (0.0f); }

    struct Out { float lp, bp, hp; };

    Out process (int ch, float v0) noexcept
    {
        auto& s1 = ic1[(size_t) ch];
        auto& s2 = ic2[(size_t) ch];
        const float v3 = v0 - s2;
        const float v1 = a1 * s1 + a2 * v3;
        const float v2 = s2 + a2 * s1 + a3 * v3;
        s1 = snapToZero (2.0f * v1 - s1);
        s2 = snapToZero (2.0f * v2 - s2);
        return { v2, v1, v0 - k * v1 - v2 };
    }

private:
    float g = 0.0f, k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    std::array<float, kMaxChannels> ic1 {}, ic2 {};
};

//==============================================================================
/** One-pole lowpass / highpass (para DC-blockers y suavizado de ruido de control). */
class OnePole
{
public:
    void setCutoff (float hz, float sampleRate) noexcept
    {
        a = std::exp (-kTwoPi * hz / sampleRate);
    }
    void reset() noexcept { z.fill (0.0f); }
    float lowpass (int ch, float x) noexcept
    {
        auto& s = z[(size_t) ch];
        s = snapToZero (x + a * (s - x));
        return s;
    }
    float highpass (int ch, float x) noexcept { return x - lowpass (ch, x); }

private:
    float a = 0.0f;
    std::array<float, kMaxChannels> z {};
};

//==============================================================================
/** Ruido de control suave (-1..1): elige un objetivo aleatorio cada
    minInterval..maxInterval segundos y lo filtra con 2 one-poles en cascada.
    Ideal para drift de motor, fluctuaciones lentas, etc. */
class SmoothRandom
{
public:
    void prepare (float sampleRate, float minIntervalSec, float maxIntervalSec, float smoothHz, uint32_t seed)
    {
        sr = sampleRate; minI = minIntervalSec; maxI = maxIntervalSec;
        a = std::exp (-kTwoPi * smoothHz / sr);
        rng.setSeed (seed);
        target = rng.bipolar(); s1 = s2 = target;
        countdown = nextInterval();
    }
    float next() noexcept
    {
        if (--countdown <= 0) { target = rng.bipolar(); countdown = nextInterval(); }
        s1 = target + a * (s1 - target);
        s2 = s1 + a * (s2 - s1);
        return s2;
    }

private:
    int nextInterval() noexcept { return juce::jmax (1, (int) ((minI + (maxI - minI) * rng.uniform()) * sr)); }
    float sr = 44100.0f, minI = 0.5f, maxI = 1.0f, a = 0.0f, target = 0.0f, s1 = 0.0f, s2 = 0.0f;
    int countdown = 1;
    FastRandom rng;
};

//==============================================================================
/** Crossfade de bypass reutilizable: wet 1 = módulo activo, 0 = bypass. */
struct BypassFade
{
    SmoothedParam amount;
    void prepare (double sr)                 { amount.prepare (sr, kBypassSeconds); }
    void setActive (bool active) noexcept    { amount.set (active ? 1.0f : 0.0f); }
    bool isFullyBypassed() const noexcept    { return ! amount.isSmoothing() && amount.target() < 0.5f && amount.current() <= 0.0f; }
    bool isFullyActive() const noexcept      { return ! amount.isSmoothing() && amount.current() >= 1.0f; }
    float next() noexcept                    { return amount.next(); }
};

} // namespace lofi
