#pragma once
//==============================================================================
//  3. CRUSH — bit depth (16..4 bits) + sample rate reduction (host sr .. 2 kHz)
//  Procesado a 2x con juce::dsp::Oversampling (FIR half-band equiripple, fase
//  lineal, latencia entera) para que el aliasing generado por la cuantización
//  y el sample&hold no se refleje sobre la banda audible más de lo deseado.
//  El dry de la mezcla se retrasa la misma latencia => mix sin filtrado peine.
//==============================================================================
#include "DSPCommon.h"

namespace lofi
{
class CrushModule
{
public:
    struct Params
    {
        bool  enabled = true;
        float bits    = 10.0f;     // 4..16 (continuo)
        float rateHz  = 12000.0f;  // 2000..48000
        float mix     = 1.0f;      // 0..1
    };

    void prepare (double sampleRate, int maxBlockSize, int numChannels)
    {
        sr  = (float) sampleRate;
        nch = juce::jmin (numChannels, kMaxChannels);
        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            (size_t) nch, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
        oversampler->initProcessing ((size_t) maxBlockSize);
        oversampler->reset();
        latency = juce::roundToInt (oversampler->getLatencyInSamples());

        dryDelay.prepare (nch, latency + 4);
        bits.prepare (sampleRate * 2.0);
        rate.prepare (sampleRate * 2.0);
        mix.prepare (sampleRate);
        bypass.prepare (sampleRate);
        holdPhase = 1.0f;
        held.fill (0.0f);
    }

    void reset()
    {
        if (oversampler) oversampler->reset();
        dryDelay.reset();
    }

    int getLatencySamples() const noexcept { return latency; }

    void setParams (const Params& p) noexcept
    {
        bypass.setActive (p.enabled);
        bits.set (juce::jlimit (4.0f, 16.0f, p.bits));
        rate.set (juce::jlimit (2000.0f, 48000.0f, p.rateHz));
        mix.set  (juce::jlimit (0.0f, 1.0f, p.mix));
    }

    void process (juce::AudioBuffer<float>& buffer, int start, int num) noexcept
    {
        const int chans = juce::jmin (buffer.getNumChannels(), nch);
        float* const* ch = buffer.getArrayOfWritePointers();

        // 1) guardar dry retrasado (alineado con la latencia del oversampler)
        //    en un scratch (sin allocations: tamaño fijo reservado abajo)
        jassert (num <= kScratch);
        for (int n = 0; n < num; ++n)
        {
            for (int c = 0; c < chans; ++c)
            {
                dryDelay.write (c, ch[c][start + n]);
                dryScratch[(size_t) c][(size_t) n] = dryDelay.read (c, (float) latency);
            }
            dryDelay.advance();
        }

        // 2) wet a 2x. Siempre corre (barato), así no hay estados viejos al salir de bypass.
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) chans,
                                            (size_t) start, (size_t) num);
        auto up = oversampler->processSamplesUp (block);
        const int upN = (int) up.getNumSamples();
        const float osSr = sr * 2.0f;

        for (int n = 0; n < upN; ++n)
        {
            const float b      = bits.next();
            const float target = juce::jmin (rate.next(), sr);
            const float levels = std::exp2 (b - 1.0f);
            const bool  doHold = target < sr * 0.999f;

            holdPhase += target / osSr;
            const bool capture = (! doHold) || holdPhase >= 1.0f;
            if (holdPhase >= 1.0f) holdPhase -= std::floor (holdPhase);

            for (int c = 0; c < chans; ++c)
            {
                float* d = up.getChannelPointer ((size_t) c);
                if (capture) held[(size_t) c] = d[n];
                float y = held[(size_t) c];
                if (b < 15.95f)                       // cuantización mid-tread
                    y = std::round (y * levels) / levels;
                d[n] = y;
            }
        }
        oversampler->processSamplesDown (block);

        // 3) mezcla dry/wet + crossfade de bypass
        for (int n = 0; n < num; ++n)
        {
            const float m = mix.next() * bypass.next();
            for (int c = 0; c < chans; ++c)
            {
                const float dry = dryScratch[(size_t) c][(size_t) n];
                ch[c][start + n] = dry + m * (ch[c][start + n] - dry);
            }
        }
    }

    static constexpr int kScratch = 8192;

private:
    float sr = 44100.0f;
    int nch = 2, latency = 0;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    DelayLine dryDelay;
    SmoothedParam bits, rate, mix;
    BypassFade bypass;
    float holdPhase = 1.0f;
    std::array<float, kMaxChannels> held {};
    std::array<std::array<float, kScratch>, kMaxChannels> dryScratch {};
};
} // namespace lofi
