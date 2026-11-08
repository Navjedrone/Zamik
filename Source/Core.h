#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

namespace zamik
{
    // 1/768 of a whole note covers every division used: 1/128 triplet = 4 ticks, 1/128 dotted = 9.
    constexpr int    kTicksPerWhole = 768;
    constexpr int    kMaxTicks      = kTicksPerWhole * 16;
    constexpr double kMaxMs         = 2000.0;   // MS / SMP range, each direction
    constexpr double kMaxLaterSec   = 4.0;      // largest "later" shift the buffer holds
    constexpr double kMaxAdvanceSec = 2.0;      // largest advance range
    constexpr double kMaxMakeupDb   = 12.0;

    constexpr int kAdvanceMs[]  = { 0, 10, 25, 50, 100, 250, 500, 1000, 2000 };
    constexpr int kNumAdvance   = 9;
    constexpr int kMsStep8[]    = { 1, 2, 4, 8, 16, 32, 64 };     // in 1/8 ms
    constexpr int kSmpStep[]    = { 1, 2, 4, 8, 16, 32, 64 };     // in samples
    constexpr int kNumSteps     = 7;
    constexpr int kNumDivs      = 9;                              // 1/128 .. 1/1, BAR
    constexpr int kNumSlots     = 4;

    extern const char* const kMsStepNames[kNumSteps];
    extern const char* const kDivNames[kNumDivs];
    extern const char* const kFeelNames[3];

    enum class Mode   { ms = 0, smp = 1, note = 2 };
    enum class ChMode { lr = 0, ms = 1 };

    /** One display's offset. MS and SMP share `ms`; NOTE adds note steps on top of a fixed base. */
    struct Lane
    {
        double ms     = 0.0;
        int    ticks  = 0;
        double baseMs = 0.0;
    };

    /** Everything one A/B/C/D variation remembers. */
    struct Settings
    {
        Lane   lanes[2];
        Mode   mode     = Mode::ms;
        bool   dual     = false;
        ChMode chm      = ChMode::lr;
        bool   sync     = false;
        int    msStep   = 3;      // index into kMsStep8 (3 = 1 ms)
        int    smpStep  = 0;
        int    div      = 3;      // 1/16
        int    feel     = 0;      // straight, triplet, dotted
        int    advIndex = 5;      // 250 ms
        int    bal      = 0;      // -100 (mid) .. +100 (side)
        bool   balAuto  = true;

        juce::ValueTree toTree() const;
        static Settings fromTree (const juce::ValueTree&);
    };

    struct HostTime
    {
        double bpm = 120.0;
        int    num = 4, den = 4;
    };

    int    barTicks (const HostTime&);
    int    divTicks (const Settings&, const HostTime&);
    double ticksToMs (double ticks, double bpm);
    double msPerTick (double bpm);

    /** The offset the user asked for, in ms (before range limits). */
    double wantedMs (const Settings&, int lane, const HostTime&);

    /** Whole-sample offset the user asked for. */
    juce::int64 wantedSamples (const Settings&, int lane, const HostTime&, double sampleRate);

    int latencySamples (int advIndex, double sampleRate);
    juce::int64 maxLaterSamples (double sampleRate);

    struct Applied { juce::int64 samples = 0; bool limited = false; };
    Applied applied (juce::int64 wanted, int latency, double sampleRate);

    /** Mid/side balance gains before makeup. */
    void balanceGains (int bal, float& gm, float& gs);

    juce::String formatSigned (juce::int64);
}
