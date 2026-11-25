//==============================================================================
//  LoFiBoxDSPTest — app de consola para probar la cadena DSP sin host ni GUI.
//
//  Uso:
//    LoFiBoxDSPTest                       -> genera señal de prueba y escribe
//                                            lofibox_test_in.wav / _out.wav
//    LoFiBoxDSPTest in.wav [out.wav]      -> procesa tu archivo
//
//  Qué verifica:
//    1. Null test: con todos los módulos en bypass la salida debe ser
//       idéntica a la entrada retrasada la latencia reportada.
//    2. Render completo con dropouts y tapestop programados, en bloques de
//       tamaño aleatorio (1..1024) para estresar el troceo sample-accurate.
//    3. Sin NaN/Inf, sin denormals, picos y "saltos" máximos entre samples.
//==============================================================================
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Source/dsp/LoFiChain.h"
#include <iostream>

using namespace lofi;

static juce::AudioBuffer<float> makeTestSignal (double sr, double seconds)
{
    const int n = (int) (sr * seconds);
    juce::AudioBuffer<float> b (2, n);
    b.clear();
    const float freqs[] = { 261.63f, 329.63f, 392.0f, 493.88f, 130.81f };
    for (int i = 0; i < n; ++i)
    {
        const double t = i / sr;
        const double beat = std::fmod (t, 1.0);
        const float env = (float) std::exp (-beat * 2.5);
        float s = 0.0f;
        for (auto f : freqs) s += std::sin (juce::MathConstants<float>::twoPi * f * (float) t);
        s *= 0.12f * env;
        // "hat" de ruido cada 0.25 s
        const double q = std::fmod (t, 0.25);
        const float hat = q < 0.03 ? (float) (std::exp (-q * 200.0) * 0.08 * (juce::Random::getSystemRandom().nextFloat() * 2 - 1)) : 0.0f;
        b.setSample (0, i, s + hat);
        b.setSample (1, i, s * 0.9f + hat);
    }
    return b;
}

static bool writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double sr)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
    if (os == nullptr) return false;
    JUCE_BEGIN_IGNORE_DEPRECATION_WARNINGS
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, (unsigned) b.getNumChannels(), 24, {}, 0));
    JUCE_END_IGNORE_DEPRECATION_WARNINGS
    if (w == nullptr) return false;
    os.release();
    return w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

struct Event { double time; int type; };   // 0 = dropout, 1 = TS down, 2 = TS up

static void render (LoFiChain& chain, juce::AudioBuffer<float>& io, double sr,
                    const std::vector<Event>& events, bool randomBlocks)
{
    juce::Random rnd (42);
    int pos = 0, ev = 0;
    const int total = io.getNumSamples();
    while (pos < total)
    {
        int len = randomBlocks ? 1 + rnd.nextInt (1024) : 512;
        len = juce::jmin (len, total - pos);
        // trocear en eventos
        int blockEnd = pos + len;
        while (pos < blockEnd)
        {
            int next = blockEnd;
            if (ev < (int) events.size())
            {
                const int evSample = (int) (events[(size_t) ev].time * sr);
                if (evSample < blockEnd) next = juce::jmax (pos, evSample);
            }
            if (next > pos) chain.process (io, pos, next - pos);
            pos = next;
            if (ev < (int) events.size() && (int) (events[(size_t) ev].time * sr) <= pos)
            {
                const auto& e = events[(size_t) ev++];
                if (e.type == 0) chain.triggerDropout (1.0f);
                if (e.type == 1) chain.setTapeStopPressed (true);
                if (e.type == 2) chain.setTapeStopPressed (false);
            }
        }
    }
}

struct Stats { float peak = 0, maxJump = 0; int nonFinite = 0, denormals = 0; };

static Stats analyse (const juce::AudioBuffer<float>& b)
{
    Stats s;
    for (int c = 0; c < b.getNumChannels(); ++c)
    {
        const float* d = b.getReadPointer (c);
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float x = d[i];
            if (! std::isfinite (x)) { ++s.nonFinite; continue; }
            if (std::fpclassify (x) == FP_SUBNORMAL) ++s.denormals;
            s.peak = std::max (s.peak, std::abs (x));
            if (i > 0) s.maxJump = std::max (s.maxJump, std::abs (x - d[i - 1]));
        }
    }
    return s;
}

int main (int argc, char* argv[])
{
    juce::ScopedNoDenormals noDenormals;

    double sr = 48000.0;
    juce::AudioBuffer<float> input;
    juce::File outFile = juce::File::getCurrentWorkingDirectory().getChildFile ("lofibox_test_out.wav");

    if (argc > 1)
    {
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1])));
        if (r == nullptr) { std::cerr << "No pude leer " << argv[1] << "\n"; return 1; }
        sr = r->sampleRate;
        input.setSize ((int) juce::jmin (2u, r->numChannels), (int) r->lengthInSamples);
        r->read (&input, 0, (int) r->lengthInSamples, 0, true, true);
        if (argc > 2) outFile = juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]);
    }
    else
    {
        input = makeTestSignal (sr, 10.0);
        writeWav (juce::File::getCurrentWorkingDirectory().getChildFile ("lofibox_test_in.wav"), input, sr);
    }

    const int nch = input.getNumChannels();
    int failures = 0;

    // ---------- 1. NULL TEST (todo en bypass)
    {
        LoFiChain chain;
        chain.prepare (sr, 512, nch);
        ChainParams p;
        p.noise.enabled = p.wow.enabled = p.crush.enabled = p.filter.enabled = false;
        p.dropout.enabled = p.tapestop.enabled = false;
        chain.setParams (p);
        auto io = input;
        render (chain, io, sr, {}, true);
        const int L = chain.getLatencySamples();
        float maxErr = 0.0f;
        for (int c = 0; c < nch; ++c)
            for (int i = L; i < io.getNumSamples(); ++i)
                maxErr = std::max (maxErr, std::abs (io.getSample (c, i) - input.getSample (c, i - L)));
        std::cout << "[null test] latencia = " << L << " samples (" << (1000.0 * L / sr) << " ms), error max = " << maxErr << "\n";
        if (maxErr > 1.0e-5f) { std::cout << "  FALLA\n"; ++failures; }
    }

    // ---------- 2. RENDER COMPLETO
    LoFiChain chain;
    chain.prepare (sr, 512, nch);
    ChainParams p;
    p.noise   = { true, -38.0f, 5.0f, 0.45f };
    p.wow     = { true, 0.55f, 14.0f, 6.5f, 3.0f, 0.4f };
    p.crush   = { true, 10.0f, 11025.0f, 0.6f };
    p.filter  = { true, 110.0f, 7000.0f, 0.25f };
    p.dropout = { true, 0.7f, DropoutModule::Mode::Auto, 8.0f };
    p.tapestop = { true, 0.8f, TapeStopModule::Curve::Exponential, TapeStopModule::Restart::OnRelease };
    chain.setParams (p);

    std::vector<Event> events = {
        { 1.5, 0 }, { 2.6, 0 }, { 2.75, 0 },     // dropouts manuales (uno re-trigger)
        { 4.0, 1 }, { 6.0, 2 },                  // tapestop momentáneo
        { 7.2, 1 }, { 7.3, 2 },                  // golpe corto: frena y vuelve antes de detenerse
        { 8.5, 1 }, { 8.6, 0 }, { 9.0, 2 }       // dropout durante tapestop
    };
    auto io = input;
    render (chain, io, sr, events, true);

    // modo retrigger (toggle) en una segunda pasada
    {
        LoFiChain c2; c2.prepare (sr, 512, nch);
        auto p2 = p; p2.tapestop.restart = TapeStopModule::Restart::OnRetrigger;
        p2.tapestop.curve = TapeStopModule::Curve::SCurve;
        c2.setParams (p2);
        auto io2 = input;
        render (c2, io2, sr, { { 2.0, 1 }, { 2.05, 2 }, { 4.0, 1 }, { 4.05, 2 } }, true);
        auto st2 = analyse (io2);
        std::cout << "[toggle]   peak " << st2.peak << "  nonFinite " << st2.nonFinite << "  denormals " << st2.denormals << "\n";
        if (st2.nonFinite || st2.denormals) ++failures;
        writeWav (outFile.getSiblingFile (outFile.getFileNameWithoutExtension() + "_toggle.wav"), io2, sr);
    }

    auto st = analyse (io);
    auto stIn = analyse (input);
    std::cout << "[render]   peak " << st.peak << " (in " << stIn.peak << ")  saltoMax " << st.maxJump
              << " (in " << stIn.maxJump << ")  nonFinite " << st.nonFinite << "  denormals " << st.denormals << "\n";
    if (st.nonFinite || st.denormals || st.peak > 4.0f) ++failures;

    writeWav (outFile, io, sr);
    std::cout << "Escrito: " << outFile.getFullPathName() << "\n";
    std::cout << (failures == 0 ? "OK" : "HAY FALLAS") << "\n";
    return failures == 0 ? 0 : 2;
}
