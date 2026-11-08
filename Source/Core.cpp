#include "Core.h"

namespace zamik
{
    const char* const kMsStepNames[kNumSteps] = { "\xc3\xb7" "8", "\xc3\xb7" "4", "\xc3\xb7" "2", "1 ms",
                                                  "\xc3\x97" "2", "\xc3\x97" "4", "\xc3\x97" "8" };
    const char* const kDivNames[kNumDivs]     = { "1/128", "1/64", "1/32", "1/16", "1/8", "1/4", "1/2", "1/1", "BAR" };
    const char* const kFeelNames[3]           = { "STRAIGHT", "TRIPLET", "DOTTED" };

    static const juce::Identifier idSettings ("Settings"), idLane ("Lane"), idMs ("ms"), idTicks ("ticks"),
        idBase ("baseMs"), idMode ("mode"), idDual ("dual"), idChm ("chm"), idSync ("sync"), idMsStep ("msStep"),
        idSmpStep ("smpStep"), idDiv ("div"), idFeel ("feel"), idAdv ("adv"), idBal ("bal"), idBalAuto ("balAuto");

    juce::ValueTree Settings::toTree() const
    {
        juce::ValueTree t (idSettings);
        t.setProperty (idMode, (int) mode, nullptr);
        t.setProperty (idDual, dual, nullptr);
        t.setProperty (idChm, (int) chm, nullptr);
        t.setProperty (idSync, sync, nullptr);
        t.setProperty (idMsStep, msStep, nullptr);
        t.setProperty (idSmpStep, smpStep, nullptr);
        t.setProperty (idDiv, div, nullptr);
        t.setProperty (idFeel, feel, nullptr);
        t.setProperty (idAdv, advIndex, nullptr);
        t.setProperty (idBal, bal, nullptr);
        t.setProperty (idBalAuto, balAuto, nullptr);
        for (auto& l : lanes)
        {
            juce::ValueTree lt (idLane);
            lt.setProperty (idMs, l.ms, nullptr);
            lt.setProperty (idTicks, l.ticks, nullptr);
            lt.setProperty (idBase, l.baseMs, nullptr);
            t.appendChild (lt, nullptr);
        }
        return t;
    }

    Settings Settings::fromTree (const juce::ValueTree& t)
    {
        Settings s;
        if (! t.hasType (idSettings))
            return s;
        auto clampI = [] (int v, int lo, int hi) { return juce::jlimit (lo, hi, v); };
        s.mode     = (Mode) clampI ((int) t.getProperty (idMode, 0), 0, 2);
        s.dual     = (bool) t.getProperty (idDual, false);
        s.chm      = (ChMode) clampI ((int) t.getProperty (idChm, 0), 0, 1);
        s.sync     = (bool) t.getProperty (idSync, false);
        s.msStep   = clampI ((int) t.getProperty (idMsStep, 3), 0, kNumSteps - 1);
        s.smpStep  = clampI ((int) t.getProperty (idSmpStep, 0), 0, kNumSteps - 1);
        s.div      = clampI ((int) t.getProperty (idDiv, 3), 0, kNumDivs - 1);
        s.feel     = clampI ((int) t.getProperty (idFeel, 0), 0, 2);
        s.advIndex = clampI ((int) t.getProperty (idAdv, 5), 0, kNumAdvance - 1);
        s.bal      = clampI ((int) t.getProperty (idBal, 0), -100, 100);
        s.balAuto  = (bool) t.getProperty (idBalAuto, true);
        for (int i = 0; i < 2 && i < t.getNumChildren(); ++i)
        {
            auto lt = t.getChild (i);
            s.lanes[i].ms     = juce::jlimit (-kMaxMs, kMaxMs, (double) lt.getProperty (idMs, 0.0));
            s.lanes[i].ticks  = clampI ((int) lt.getProperty (idTicks, 0), -kMaxTicks, kMaxTicks);
            s.lanes[i].baseMs = (double) lt.getProperty (idBase, 0.0);
        }
        return s;
    }

    int barTicks (const HostTime& h)
    {
        return juce::roundToInt (kTicksPerWhole * (double) h.num / (double) juce::jmax (1, h.den));
    }

    int divTicks (const Settings& s, const HostTime& h)
    {
        if (s.div == 8)
            return juce::jmax (1, barTicks (h));
        const int base = kTicksPerWhole >> (7 - s.div);            // 1/128 -> 6 ... 1/1 -> 768
        return s.feel == 1 ? base * 2 / 3 : s.feel == 2 ? base * 3 / 2 : base;
    }

    double msPerTick (double bpm)              { return 4.0 * 60000.0 / (kTicksPerWhole * bpm); }
    double ticksToMs (double ticks, double bpm) { return ticks * msPerTick (bpm); }

    double wantedMs (const Settings& s, int lane, const HostTime& h)
    {
        const auto& l = s.lanes[lane];
        return s.mode == Mode::note ? ticksToMs (l.ticks, h.bpm) + l.baseMs : l.ms;
    }

    juce::int64 wantedSamples (const Settings& s, int lane, const HostTime& h, double sr)
    {
        return (juce::int64) std::llround (wantedMs (s, lane, h) * sr / 1000.0);
    }

    int latencySamples (int advIndex, double sr)
    {
        return (int) std::llround (kAdvanceMs[juce::jlimit (0, kNumAdvance - 1, advIndex)] * sr / 1000.0);
    }

    juce::int64 maxLaterSamples (double sr) { return (juce::int64) std::llround (kMaxLaterSec * sr); }

    Applied applied (juce::int64 wanted, int latency, double sr)
    {
        Applied a;
        a.samples = juce::jlimit ((juce::int64) -latency, maxLaterSamples (sr), wanted);
        a.limited = a.samples != wanted;
        return a;
    }

    void balanceGains (int bal, float& gm, float& gs)
    {
        // left of center turns side down (toward mono), right turns mid down (toward side only)
        gm = bal > 0 ? 1.0f - (float) bal / 100.0f : 1.0f;
        gs = bal < 0 ? 1.0f + (float) bal / 100.0f : 1.0f;
    }

    juce::String formatSigned (juce::int64 v)
    {
        return (v > 0 ? "+" : "") + juce::String (v);
    }
}
