#pragma once
//==============================================================================
//  LoFiChain — la cadena completa, independiente de AudioProcessor.
//  Orden: NOISE -> WOW&FLUTTER -> CRUSH -> FILTER -> DROPOUTS -> TAPESTOP
//         -> MIX global (dry compensado en latencia) -> OUTPUT
//  (el ruido entra antes del wow para que el hiss también "flamee" con la
//   cinta, y antes del tapestop para que el ruido también se frene.)
//==============================================================================
#include "NoiseModule.h"
#include "WowFlutterModule.h"
#include "CrushModule.h"
#include "FilterModule.h"
#include "DropoutModule.h"
#include "TapeStopModule.h"

namespace lofi
{
struct ChainParams
{
    NoiseModule::Params      noise;
    WowFlutterModule::Params wow;
    CrushModule::Params      crush;
    FilterModule::Params     filter;
    DropoutModule::Params    dropout;
    TapeStopModule::Params   tapestop;
    float mix      = 1.0f;   // 0..1
    float outputDb = 0.0f;   // -24..+12
};

class LoFiChain
{
public:
    void prepare (double sampleRate, int maxBlockSize, int numChannels)
    {
        numCh = juce::jlimit (1, kMaxChannels, numChannels);
        maxBlock = juce::jlimit (16, CrushModule::kScratch, maxBlockSize);
        noise.prepare (sampleRate, numCh);
        wow.prepare (sampleRate, numCh);
        crush.prepare (sampleRate, maxBlock, numCh);
        filter.prepare (sampleRate, numCh);
        dropout.prepare (sampleRate, numCh);
        tapestop.prepare (sampleRate, numCh);

        latency = wow.getLatencySamples() + crush.getLatencySamples();
        dryDelay.prepare (numCh, latency + 4);
        mix.prepare (sampleRate);
        outGain.prepare (sampleRate);
    }

    void reset()
    {
        wow.reset(); crush.reset(); filter.reset(); dropout.reset(); tapestop.reset();
        dryDelay.reset();
    }

    int getLatencySamples() const noexcept { return latency; }

    void setParams (const ChainParams& p) noexcept
    {
        noise.setParams (p.noise);
        wow.setParams (p.wow);
        crush.setParams (p.crush);
        filter.setParams (p.filter);
        dropout.setParams (p.dropout);
        tapestop.setParams (p.tapestop);
        mix.set (juce::jlimit (0.0f, 1.0f, p.mix));
        outGain.set (dbToGain (juce::jlimit (-60.0f, 24.0f, p.outputDb)));
    }

    // --- eventos (llamar entre sub-bloques para precisión de sample)
    void triggerDropout (float velocity = 1.0f) noexcept { dropout.trigger (velocity); }
    void setTapeStopPressed (bool down) noexcept         { tapestop.setPressed (down); }

    /** Procesa [start, start+num). Internamente trocea a maxBlock. */
    void process (juce::AudioBuffer<float>& buffer, int start, int num) noexcept
    {
        while (num > 0)
        {
            const int n = juce::jmin (num, maxBlock);
            processChunk (buffer, start, n);
            start += n; num -= n;
        }
    }

    // --- info para la GUI
    float getTapeSpeed() const noexcept      { return tapestop.getSpeed() * wow.getCurrentSpeed(); }
    TapeStopModule::State getTapeState() const noexcept { return tapestop.getState(); }
    float getDropoutActivity() const noexcept { return dropout.getActivity(); }

    NoiseModule noise;
    WowFlutterModule wow;
    CrushModule crush;
    FilterModule filter;
    DropoutModule dropout;
    TapeStopModule tapestop;

private:
    void processChunk (juce::AudioBuffer<float>& buffer, int start, int num) noexcept
    {
        const int chans = juce::jmin (buffer.getNumChannels(), numCh);
        float* const* ch = buffer.getArrayOfWritePointers();

        // dry compensado (latencia total de la cadena)
        for (int i = 0; i < num; ++i)
        {
            for (int c = 0; c < chans; ++c)
            {
                dryDelay.write (c, ch[c][start + i]);
                dry[(size_t) c][(size_t) i] = dryDelay.read (c, (float) latency);
            }
            dryDelay.advance();
        }

        noise.process (buffer, start, num);
        wow.process (buffer, start, num);
        crush.process (buffer, start, num);
        filter.process (buffer, start, num);
        dropout.process (buffer, start, num);
        tapestop.process (buffer, start, num);

        for (int i = 0; i < num; ++i)
        {
            const float m = mix.next();
            const float g = outGain.next();
            for (int c = 0; c < chans; ++c)
            {
                const float d = dry[(size_t) c][(size_t) i];
                float y = (d + m * (ch[c][start + i] - d)) * g;
                if (! std::isfinite (y)) y = 0.0f;   // red de seguridad
                ch[c][start + i] = y;
            }
        }
    }

    int numCh = 2, maxBlock = 512, latency = 0;
    DelayLine dryDelay;
    SmoothedParam mix, outGain;
    std::array<std::array<float, CrushModule::kScratch>, kMaxChannels> dry {};
};
} // namespace lofi
