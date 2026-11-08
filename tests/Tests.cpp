// Offline test runner for Zamik.
// Feeds random 32/64-bit words (every float class: NaN payloads, denormals, infinities, signed zeros)
// through the real processor in uneven block sizes, then checks every output word against the
// input moved by exactly (reported latency + offset) samples.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <random>

using namespace zamik;

struct FixedPlayHead : juce::AudioPlayHead
{
    double bpm = 120.0; int num = 4, den = 4;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm);
        p.setTimeSignature (TimeSignature { num, den });
        return p;
    }
};

static int failures = 0, checks = 0;
static juce::int64 wordsCompared = 0;

static void report (const juce::String& name, bool pass, const juce::String& detail = {})
{
    ++checks;
    if (! pass) ++failures;
    std::cout << (pass ? "PASS  " : "FAIL  ") << name << (detail.isNotEmpty() ? "  |  " + detail : juce::String()) << "\n";
}

template <typename T> struct Word;
template <> struct Word<float>  { using U = uint32_t; };
template <> struct Word<double> { using U = uint64_t; };

template <typename T>
static std::vector<std::vector<T>> makeSignal (int numCh, int len, uint64_t seed)
{
    using U = typename Word<T>::U;
    std::mt19937_64 rng (seed);
    std::vector<std::vector<T>> s ((size_t) numCh, std::vector<T> ((size_t) len));
    const U specials32[] = { 0x00000000u, 0x80000000u, 0x7f800000u, 0xff800000u, 0x7fc00000u, 0x7f800001u, 0x7fbfffffu,
                             0xffc12345u, 0x00000001u, 0x807fffffu, 0x00400000u, 0x3f800000u, 0xbf800000u, 0x7f7fffffu };
    for (auto& ch : s)
    {
        for (auto& v : ch) { U u = (U) rng(); std::memcpy (&v, &u, sizeof (T)); }
        for (size_t i = 0; i < std::size (specials32); ++i)
        {
            U u;
            if constexpr (sizeof (T) == 4) u = specials32[i];
            else                           u = ((U) specials32[i] << 32) | (U) specials32[i];
            std::memcpy (&ch[(i * 997 + 3) % (size_t) len], &u, sizeof (T));
        }
    }
    return s;
}

/** Runs `len` samples through the processor in uneven blocks and returns the output. */
template <typename T>
static std::vector<std::vector<T>> run (ZamikProcessor& p, const std::vector<std::vector<T>>& in, bool hostBypass = false)
{
    const int numCh = (int) in.size(), len = (int) in[0].size();
    std::vector<std::vector<T>> out ((size_t) numCh, std::vector<T> ((size_t) len));
    const int blocks[] = { 512, 17, 4096, 1, 9000, 333, 64, 4097 };
    juce::MidiBuffer midi;
    int pos = 0, b = 0;
    while (pos < len)
    {
        const int n = juce::jmin (blocks[b++ % std::size (blocks)], len - pos);
        juce::AudioBuffer<T> buf (numCh, n);
        for (int c = 0; c < numCh; ++c) std::memcpy (buf.getWritePointer (c), in[(size_t) c].data() + pos, (size_t) n * sizeof (T));
        if (hostBypass) p.processBlockBypassed (buf, midi);
        else            p.processBlock (buf, midi);
        for (int c = 0; c < numCh; ++c) std::memcpy (out[(size_t) c].data() + pos, buf.getReadPointer (c), (size_t) n * sizeof (T));
        pos += n;
    }
    return out;
}

/** Counts words where out[n] != in[n - d] (n >= d) or out[n] != 0 (n < d, the ring starts silent). */
template <typename T>
static juce::int64 mismatches (const std::vector<T>& in, const std::vector<T>& out, int d)
{
    using U = typename Word<T>::U;
    juce::int64 bad = 0;
    for (size_t n = 0; n < out.size(); ++n)
    {
        U a, b = 0;
        std::memcpy (&a, &out[n], sizeof (T));
        if ((int) n >= d) std::memcpy (&b, &in[n - (size_t) d], sizeof (T));
        if (a != b) ++bad;
    }
    wordsCompared += (juce::int64) out.size();
    return bad;
}

template <typename T>
static void fresh (ZamikProcessor& p, double sr)
{
    p.setProcessingPrecision (std::is_same_v<T, double> ? juce::AudioProcessor::doublePrecision : juce::AudioProcessor::singlePrecision);
    p.setRateAndBufferSizeDetails (sr, 512);
    p.prepareToPlay (sr, 512);
}

template <typename T>
static void expectShift (const juce::String& name, ZamikProcessor& p, double sr, int d0, int d1, int len = 300000, bool hostBypass = false)
{
    fresh<T> (p, sr);
    auto in = makeSignal<T> (2, len, (uint64_t) (d0 * 31 + d1 * 7 + len));
    auto out = run (p, in, hostBypass);
    const auto bl = mismatches (in[0], out[0], d0), br = mismatches (in[1], out[1], d1);
    report (name, bl == 0 && br == 0,
            "L delay " + juce::String (d0) + ", R delay " + juce::String (d1) + ", mismatches " + juce::String (bl) + "/" + juce::String (br));
}

static void resetSettings (ZamikProcessor& p)
{
    p.setBypass (false);
    if (p.settings().mode != Mode::ms) p.setMode (Mode::ms);
    p.setDual (false); p.setChMode (ChMode::lr); p.setSync (false); p.setBalance (0); p.zero();
    p.setMsStep (3); p.setSmpStep (0);
}

template <typename T>
static void suite (const char* label)
{
    std::cout << "\n== " << label << " ==\n";
    ZamikProcessor p;
    FixedPlayHead head;
    p.setPlayHead (&head);

    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        const juce::String at = " @ " + juce::String (sr / 1000.0, 1) + " kHz";
        for (int adv : { 0, 5, 8 })
        {
            resetSettings (p);
            p.setAdvance (adv);
            fresh<T> (p, sr);
            const int lat = latencySamples (adv, sr);
            report ("latency reported, range " + juce::String (kAdvanceMs[adv]) + " ms" + at, p.getLatencySamples() == lat, juce::String (p.getLatencySamples()));
            expectShift<T> ("zero offset = latency only, range " + juce::String (kAdvanceMs[adv]) + " ms" + at, p, sr, lat, lat);
        }

        p.setAdvance (5);
        const int lat = latencySamples (5, sr);

        // SMP steps
        resetSettings (p); p.setMode (Mode::smp); fresh<T> (p, sr);
        for (int k = 0; k < 7; ++k) p.nudge (+1, 0);
        expectShift<T> ("SMP +7" + at, p, sr, lat + 7, lat + 7);
        p.setSmpStep (6);
        for (int k = 0; k < 10; ++k) p.nudge (-1, 0);
        expectShift<T> ("SMP -633 (range limited to -" + juce::String (lat) + ")" + at, p, sr, lat + (int) applied (-633, lat, sr).samples, lat + (int) applied (-633, lat, sr).samples);

        // MS steps
        resetSettings (p); fresh<T> (p, sr);
        p.setMsStep (0);
        for (int k = 0; k < 3; ++k) p.nudge (+1, 0);                    // +0.375 ms
        const int ms375 = (int) std::llround (0.375 * sr / 1000.0);
        expectShift<T> ("MS +0.375 ms" + at, p, sr, lat + ms375, lat + ms375);

        // NOTE with tempo and meter from the host
        head.bpm = 97.3; head.num = 7; head.den = 8;
        resetSettings (p); fresh<T> (p, sr);
        run (p, makeSignal<T> (2, 64, 1));                              // let the processor read the play head
        p.setMode (Mode::note); p.setDiv (4); p.setFeel (1);            // 1/8 triplet
        p.nudge (+1, 0); p.nudge (+1, 0);
        const auto w = (int) std::llround (ticksToMs (2 * 64, 97.3) * sr / 1000.0);
        expectShift<T> ("NOTE 2 x 1/8T @ 97.3 BPM" + at, p, sr, lat + w, lat + w);
        p.setDiv (8); p.setFeel (0); p.nudge (-1, 0);                   // minus one 7/8 bar
        const auto w2 = (juce::int64) std::llround ((ticksToMs (2 * 64, 97.3) - ticksToMs (672, 97.3)) * sr / 1000.0);
        const auto a2 = applied (w2, lat, sr).samples;
        expectShift<T> ("NOTE minus 1 bar of 7/8 (limited: " + juce::String ((int) (a2 != w2)) + ")" + at, p, sr, lat + (int) a2, lat + (int) a2);
        head.bpm = 120.0; head.num = 4; head.den = 4;

        // base delay carried into NOTE mode
        resetSettings (p); fresh<T> (p, sr);
        run (p, makeSignal<T> (2, 64, 2));
        p.setMode (Mode::smp);
        for (int k = 0; k < 5; ++k) p.nudge (+1, 0);
        p.setMode (Mode::note); p.setDiv (3); p.setFeel (0); p.nudge (+1, 0);
        const auto w3 = (int) std::llround ((ticksToMs (48, 120.0) + 5.0 * 1000.0 / sr) * sr / 1000.0);
        expectShift<T> ("NOTE 1/16 on top of a 5-sample base" + at, p, sr, lat + w3, lat + w3);

        // DUAL L/R
        resetSettings (p); p.setMode (Mode::smp); p.setDual (true); fresh<T> (p, sr);
        p.setSmpStep (5); p.nudge (+1, 0); p.nudge (-1, 1); p.nudge (-1, 1);
        expectShift<T> ("DUAL L/R +32 / -64" + at, p, sr, lat + 32, lat - 64);

        // SYNC keeps the gap
        p.setSync (true); p.nudge (+1, 0); p.nudge (+1, 1);
        expectShift<T> ("SYNC master +32 moves both, slave locked" + at, p, sr, lat + 64, lat - 32);

        // DUAL M/S with equal offsets: matrix bypassed
        resetSettings (p); p.setMode (Mode::smp); p.setDual (true); p.setChMode (ChMode::ms); p.setSync (true); fresh<T> (p, sr);
        p.nudge (+1, 0); p.nudge (+1, 0);
        report ("M/S equal offsets: matrix off" + at, ! p.matrixActive());
        expectShift<T> ("M/S equal offsets +2 / +2" + at, p, sr, lat + 2, lat + 2);

        // DUAL M/S unequal: matrix on, not transparent (expected) and flagged
        p.setSync (false); p.nudge (+1, 1);
        {
            fresh<T> (p, sr);
            auto in = makeSignal<T> (2, 50000, 9);
            for (auto& ch : in) for (auto& v : ch) if (! std::isfinite ((double) v)) v = 0;     // keep it to real audio values
            auto out = run (p, in);
            const auto bad = mismatches (in[0], out[0], lat + 2);
            report ("M/S unequal offsets: matrix flagged and output differs (expected)" + at, p.matrixActive() && bad > 0,
                    juce::String (bad) + " samples differ from a plain shift");
        }

        // bypass: latency only, whatever the settings
        resetSettings (p); p.setMode (Mode::smp); p.setDual (true); p.setChMode (ChMode::ms); p.setBalance (60); p.nudge (+1, 1);
        p.setBypass (true);
        report ("bypass: matrix off" + at, ! p.matrixActive());
        expectShift<T> ("bypass parameter: latency only" + at, p, sr, lat, lat);
        p.setBypass (false);
        expectShift<T> ("host bypass (processBlockBypassed): latency only" + at, p, sr, lat, lat, 100000, true);
    }
    p.setPlayHead (nullptr);
}

static void stateRoundTrip()
{
    std::cout << "\n== state ==\n";
    ZamikProcessor a;
    a.setMode (Mode::smp); a.setDual (true); a.setChMode (ChMode::ms); a.setSmpStep (4);
    a.nudge (+1, 0); a.nudge (-1, 1); a.setBalance (-35); a.setAdvance (7);
    a.pickSlot (2); a.nudge (+1, 0); a.pickSlot (0);
    juce::MemoryBlock mb;
    a.getStateInformation (mb);
    ZamikProcessor b;
    b.setStateInformation (mb.getData(), (int) mb.getSize());
    report ("state: current settings restored", a.settings().toTree().isEquivalentTo (b.settings().toTree()));
    report ("state: variation C restored", b.slotUsed (2) && b.activeSlot() == 0);
    b.pickSlot (2);
    a.pickSlot (2);
    report ("state: variation C contents", a.settings().toTree().isEquivalentTo (b.settings().toTree()));
}

static void snapshots (const juce::File& dir)
{
    std::cout << "\n== editor snapshots ==\n";
    juce::ScopedJuceInitialiser_GUI gui;
    ZamikProcessor p;
    p.setRateAndBufferSizeDetails (48000.0, 512);
    p.prepareToPlay (48000.0, 512);
    auto save = [&] (const juce::String& name)
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        auto* z = dynamic_cast<ZamikEditor*> (ed.get());
        juce::ignoreUnused (z);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.5f);
        juce::FileOutputStream os (dir.getChildFile (name + ".png"));
        os.setPosition (0); os.truncate();
        juce::PNGImageFormat().writeImageToStream (img, os);
        report ("snapshot " + name, img.isValid(), juce::String (ed->getWidth()) + " x " + juce::String (ed->getHeight()));
    };
    p.nudge (+1, 0); for (int i = 0; i < 12; ++i) p.nudge (+1, 0);
    save ("single_ms");
    p.setMode (Mode::note); p.nudge (+1, 0);
    save ("single_note");
    p.setMode (Mode::smp); p.setDual (true); p.setChMode (ChMode::ms); p.setBalance (-40); p.nudge (-1, 1);
    save ("dual_ms");
    p.setAdvance (0); p.nudge (-1, 0); p.nudge (-1, 0);
    save ("dual_limit");
    p.setBypass (true);
    save ("bypass");
}

int main (int argc, char** argv)
{
    suite<float>  ("32-bit float");
    suite<double> ("64-bit double");
    stateRoundTrip();
    if (argc > 1)
        snapshots (juce::File (juce::String (argv[1])));
    std::cout << "\n" << checks << " checks, " << failures << " failed, " << wordsCompared << " words compared\n";
    return failures == 0 ? 0 : 1;
}
