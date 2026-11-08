#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace zamik;

namespace
{
    constexpr int kChunk = 4096;

    template <typename T>
    void writeRing (T* ring, int ringLen, int pos, const T* src, int len)
    {
        const int first = juce::jmin (len, ringLen - pos);
        std::memcpy (ring + pos, src, (size_t) first * sizeof (T));
        if (len > first)
            std::memcpy (ring, src + first, (size_t) (len - first) * sizeof (T));
    }

    template <typename T>
    void readRing (const T* ring, int ringLen, int start, T* dst, int len)
    {
        if (start < 0) start += ringLen;
        const int first = juce::jmin (len, ringLen - start);
        std::memcpy (dst, ring + start, (size_t) first * sizeof (T));
        if (len > first)
            std::memcpy (dst + first, ring, (size_t) (len - first) * sizeof (T));
    }

    const juce::Identifier idState ("ZamikState"), idActive ("active"), idBypass ("bypass"),
                           idCurrent ("Current"), idSlot ("Slot"), idIndex ("index");
}

//==============================================================================
ZamikProcessor::ZamikProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    addParameter (bypassParam = new juce::AudioParameterBool ({ "bypass", 1 }, "Bypass", false));
    bypassParam->addListener (this);
    for (int i = 0; i < 2; ++i) { aMs[i] = 0.0; aBase[i] = 0.0; aTicks[i] = 0; }
    publish();
}

ZamikProcessor::~ZamikProcessor()
{
    bypassParam->removeListener (this);
}

bool ZamikProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    return ! in.isDisabled() && in == out && in.size() <= 8;
}

void ZamikProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    ringLen = (int) std::ceil ((kMaxAdvanceSec + kMaxLaterSec) * sampleRate) + 4 * kChunk;
    writePos = 0;
    eM = eS = 0.0;

    const int nCh = juce::jmax (getTotalNumInputChannels(), getTotalNumOutputChannels(), 1);
    if (isUsingDoublePrecision())
    {
        ringsD.io.setSize (nCh, ringLen); ringsD.io.clear();
        ringsD.ms.setSize (2, ringLen);   ringsD.ms.clear();
        ringsD.scratch.setSize (2, kChunk);
        ringsF.io.setSize (0, 0); ringsF.ms.setSize (0, 0);
    }
    else
    {
        ringsF.io.setSize (nCh, ringLen); ringsF.io.clear();
        ringsF.ms.setSize (2, ringLen);   ringsF.ms.clear();
        ringsF.scratch.setSize (2, kChunk);
        ringsD.io.setSize (0, 0); ringsD.ms.setSize (0, 0);
    }
    publish();
}

void ZamikProcessor::releaseResources()
{
    ringsF.io.setSize (0, 0); ringsF.ms.setSize (0, 0);
    ringsD.io.setSize (0, 0); ringsD.ms.setSize (0, 0);
}

template <> ZamikProcessor::Rings<float>&  ZamikProcessor::rings<float>()  { return ringsF; }
template <> ZamikProcessor::Rings<double>& ZamikProcessor::rings<double>() { return ringsD; }

void ZamikProcessor::processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer&)          { process (b, false); }
void ZamikProcessor::processBlock (juce::AudioBuffer<double>& b, juce::MidiBuffer&)         { process (b, false); }
void ZamikProcessor::processBlockBypassed (juce::AudioBuffer<float>& b, juce::MidiBuffer&)  { process (b, true); }
void ZamikProcessor::processBlockBypassed (juce::AudioBuffer<double>& b, juce::MidiBuffer&) { process (b, true); }

template <typename T>
void ZamikProcessor::process (juce::AudioBuffer<T>& buffer, bool forceBypass)
{
    // No ScopedNoDenormals on purpose: nothing here does arithmetic on the copy path,
    // and denormal samples must come out exactly as they went in.
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm())
                if (*bpm > 1.0) aBpm = *bpm;
            if (auto ts = pos->getTimeSignature())
                if (ts->numerator > 0 && ts->denominator > 0) { aNum = ts->numerator; aDen = ts->denominator; }
        }

    auto& R = rings<T>();
    const int n = buffer.getNumSamples();
    const int nCh = juce::jmin (buffer.getNumChannels(), R.io.getNumChannels());
    if (ringLen == 0 || nCh == 0 || n == 0 || R.scratch.getNumSamples() < kChunk)
        return;

    const double rate = sr.load();
    const int lat = latencyNow.load();
    const double bpm = aBpm.load();
    const bool noteMode = aMode.load() == (int) Mode::note;

    auto laneDelay = [&] (int i) -> int
    {
        const double ms = noteMode ? ticksToMs (aTicks[i].load(), bpm) + aBase[i].load() : aMs[i].load();
        const auto wanted = (juce::int64) std::llround (ms * rate / 1000.0);
        return lat + (int) applied (wanted, lat, rate).samples;
    };

    const bool bypassed = forceBypass || bypassParam->get();
    int d0 = bypassed ? lat : laneDelay (0);
    int d1 = bypassed ? lat : laneDelay (1);
    const int maxD = ringLen - kChunk - 1;
    d0 = juce::jlimit (0, maxD, d0);
    d1 = juce::jlimit (0, maxD, d1);

    const bool dual   = ! bypassed && aDual.load() != 0 && nCh >= 2;
    const bool lrMode = dual && aChm.load() == (int) ChMode::lr;
    const bool matrix = dual && aChm.load() == (int) ChMode::ms && (d0 != d1 || aBalOff.load() != 0);
    const T gm = (T) aGm.load(), gs = (T) aGs.load();

    double sumM = 0.0, sumS = 0.0;
    T* mid  = R.scratch.getWritePointer (0);
    T* side = R.scratch.getWritePointer (1);

    for (int start = 0; start < n; start += kChunk)
    {
        const int len = juce::jmin (kChunk, n - start);

        // 1. store the input (always, so switching paths never reads stale audio)
        for (int ch = 0; ch < nCh; ++ch)
            writeRing (R.io.getWritePointer (ch), ringLen, writePos, buffer.getReadPointer (ch) + start, len);

        if (nCh >= 2)
        {
            const T* l = buffer.getReadPointer (0) + start;
            const T* r = buffer.getReadPointer (1) + start;
            for (int i = 0; i < len; ++i)
            {
                mid[i]  = (l[i] + r[i]) * (T) 0.5;
                side[i] = (l[i] - r[i]) * (T) 0.5;
                if (std::isfinite ((double) mid[i]) && std::isfinite ((double) side[i]))
                {
                    sumM += (double) mid[i] * (double) mid[i];
                    sumS += (double) side[i] * (double) side[i];
                }
            }
            writeRing (R.ms.getWritePointer (0), ringLen, writePos, mid, len);
            writeRing (R.ms.getWritePointer (1), ringLen, writePos, side, len);
        }

        // 2. read it back, delayed: a plain copy for every channel on the copy path
        for (int ch = 0; ch < nCh; ++ch)
        {
            if (matrix && ch < 2)
                continue;
            const int d = (lrMode && ch == 1) ? d1 : d0;
            readRing (R.io.getReadPointer (ch), ringLen, writePos - d, buffer.getWritePointer (ch) + start, len);
        }

        // 3. mid/side with unequal offsets or balance: the only path that does arithmetic
        if (matrix)
        {
            readRing (R.ms.getReadPointer (0), ringLen, writePos - d0, mid, len);
            readRing (R.ms.getReadPointer (1), ringLen, writePos - d1, side, len);
            T* l = buffer.getWritePointer (0) + start;
            T* r = buffer.getWritePointer (1) + start;
            for (int i = 0; i < len; ++i)
            {
                const T m = mid[i] * gm, s = side[i] * gs;
                l[i] = m + s;
                r[i] = m - s;
            }
        }

        writePos = (writePos + len) % ringLen;
    }

    // long-term mid/side energy (about 3 s), used only when the balance slider moves
    if (nCh >= 2)
    {
        const double k = std::exp (-(double) n / (3.0 * rate));
        eM = eM * k + (1.0 - k) * sumM / n;
        eS = eS * k + (1.0 - k) * sumS / n;
        energyM = eM; energyS = eS;
    }
}

template void ZamikProcessor::process<float>  (juce::AudioBuffer<float>&, bool);
template void ZamikProcessor::process<double> (juce::AudioBuffer<double>&, bool);

//==============================================================================
void ZamikProcessor::publish()
{
    const int lat = zamik::latencySamples (cur.advIndex, sr.load());
    latencyNow = lat;
    if (getLatencySamples() != lat)
        setLatencySamples (lat);

    aMode   = (int) cur.mode;
    aDual   = cur.dual ? 1 : 0;
    aChm    = (int) cur.chm;
    aBalOff = cur.bal != 0 ? 1 : 0;
    for (int i = 0; i < 2; ++i)
    {
        aMs[i]    = cur.lanes[i].ms;
        aTicks[i] = cur.lanes[i].ticks;
        aBase[i]  = cur.lanes[i].baseMs;
    }
    changed();
}

void ZamikProcessor::recomputeMakeup()
{
    float gm, gs;
    balanceGains (cur.bal, gm, gs);
    float mk = 1.0f;
    if (cur.balAuto && cur.bal != 0)
    {
        double em = energyM.load(), es = energyS.load();
        if (em + es < 1e-12) { em = 1.0; es = 0.1; }      // no audio heard yet: assume a typical mid-heavy mix
        const double after = (double) gm * gm * em + (double) gs * gs * es;
        const double cap = std::pow (10.0, kMaxMakeupDb / 20.0);
        mk = after > 0.0 ? (float) juce::jmin (std::sqrt ((em + es) / after), cap) : 1.0f;
    }
    makeupDbNow = 20.0f * std::log10 (mk);
    aGm = gm * mk;
    aGs = gs * mk;
}

HostTime ZamikProcessor::hostTime() const
{
    return { aBpm.load(), aNum.load(), aDen.load() };
}

Applied ZamikProcessor::appliedFor (int lane) const
{
    const double rate = sr.load();
    return applied (wantedSamples (cur, lane, hostTime(), rate), latencyNow.load(), rate);
}

bool ZamikProcessor::matrixActive() const
{
    return ! isBypassed() && cur.dual && cur.chm == ChMode::ms
        && (appliedFor (0).samples != appliedFor (1).samples || cur.bal != 0);
}

int ZamikProcessor::suggestedAdvance() const
{
    const double rate = sr.load();
    juce::int64 needEarly = 0;
    for (int i = 0; i < (cur.dual ? 2 : 1); ++i)
    {
        const auto w = wantedSamples (cur, i, hostTime(), rate);
        if (w < -latencyNow.load())
            needEarly = juce::jmax (needEarly, -w);
    }
    if (needEarly == 0)
        return -1;
    for (int i = 0; i < kNumAdvance; ++i)
        if (zamik::latencySamples (i, rate) >= needEarly)
            return i;
    return -2;
}

//==============================================================================
void ZamikProcessor::nudge (int dir, int lane)
{
    if (! cur.dual) lane = 0;
    const bool synced = cur.dual && cur.sync;
    if (synced && lane == 1)
        return;                               // the lower display follows the upper one while synced

    const double rate = sr.load();
    const auto h = hostTime();

    if (cur.mode == Mode::note)
    {
        // note steps are added on top of the base delay, which stays as it is
        const int step = dir * divTicks (cur, h);
        const int m = synced ? 0 : lane;
        if (std::abs (cur.lanes[m].ticks + step) > kMaxTicks)
            return;
        if (synced && std::abs (cur.lanes[1].ticks + step) > kMaxTicks)
            return;

        const auto before0 = wantedSamples (cur, m, h, rate);
        cur.lanes[m].ticks += step;
        if (synced)
        {
            // the slave moves by exactly the same number of samples, so the gap never changes
            const auto dS = wantedSamples (cur, 0, h, rate) - before0;
            const auto before1 = wantedSamples (cur, 1, h, rate);
            cur.lanes[1].ticks += step;
            const auto moved1 = wantedSamples (cur, 1, h, rate) - before1;
            cur.lanes[1].baseMs += (double) (dS - moved1) * 1000.0 / rate;
        }
    }
    else
    {
        const double step = dir * (cur.mode == Mode::ms ? kMsStep8[cur.msStep] / 8.0
                                                        : kSmpStep[cur.smpStep] * 1000.0 / rate);
        const int lanes[2] = { synced ? 0 : lane, 1 };
        const int count = synced ? 2 : 1;
        for (int k = 0; k < count; ++k)
            if (std::abs (cur.lanes[lanes[k]].ms + step) > kMaxMs + 1e-9)
                return;                       // move both or neither
        for (int k = 0; k < count; ++k)
        {
            auto& v = cur.lanes[lanes[k]].ms;
            v += step;
            if (cur.mode == Mode::smp)        // keep SMP values on whole samples
                v = (double) std::llround (v * rate / 1000.0) * 1000.0 / rate;
        }
    }
    publish();
}

void ZamikProcessor::setMode (Mode m)
{
    if (m == cur.mode) return;
    const auto h = hostTime();
    const double rate = sr.load();
    for (int i = 0; i < 2; ++i)
    {
        const double w = wantedMs (cur, i, h);
        auto& l = cur.lanes[i];
        if (m == Mode::note)
        {
            l.ticks = 0;                      // the current delay becomes a fixed base
            l.baseMs = w;
        }
        else
        {
            l.ms = juce::jlimit (-kMaxMs, kMaxMs, w);
            if (m == Mode::smp)
                l.ms = (double) std::llround (l.ms * rate / 1000.0) * 1000.0 / rate;
        }
    }
    cur.mode = m;
    publish();
}

void ZamikProcessor::zero()
{
    for (auto& l : cur.lanes) l = {};
    publish();
}

void ZamikProcessor::snap()
{
    if (cur.mode != Mode::note) return;
    const auto h = hostTime();
    const int d = divTicks (cur, h);
    for (int i = 0; i < (cur.dual ? 2 : 1); ++i)
    {
        auto& l = cur.lanes[i];
        const double pos = l.ticks + l.baseMs / msPerTick (h.bpm);
        l.ticks = juce::jlimit (-kMaxTicks, kMaxTicks, (int) std::llround (pos / d) * d);
        l.baseMs = 0.0;
    }
    publish();
}

bool ZamikProcessor::canSnap() const
{
    if (cur.mode != Mode::note) return false;
    const auto h = hostTime();
    const int d = divTicks (cur, h);
    for (int i = 0; i < (cur.dual ? 2 : 1); ++i)
    {
        const auto& l = cur.lanes[i];
        const double pos = l.ticks + l.baseMs / msPerTick (h.bpm);
        if (l.baseMs != 0.0 || (int) std::llround (pos / d) * d != l.ticks)
            return true;
    }
    return false;
}

void ZamikProcessor::setDual (bool b)         { cur.dual = b; publish(); }
void ZamikProcessor::setChMode (ChMode c)     { cur.chm = c; publish(); }
void ZamikProcessor::setSync (bool b)         { cur.sync = b; publish(); }
void ZamikProcessor::setMsStep (int i)        { cur.msStep = juce::jlimit (0, kNumSteps - 1, i); changed(); }
void ZamikProcessor::setSmpStep (int i)       { cur.smpStep = juce::jlimit (0, kNumSteps - 1, i); changed(); }
void ZamikProcessor::setDiv (int i)           { cur.div = juce::jlimit (0, kNumDivs - 1, i); changed(); }
void ZamikProcessor::setFeel (int i)          { cur.feel = juce::jlimit (0, 2, i); changed(); }
void ZamikProcessor::setAdvance (int i)       { cur.advIndex = juce::jlimit (0, kNumAdvance - 1, i); publish(); }
void ZamikProcessor::setBalance (int v)       { cur.bal = juce::jlimit (-100, 100, v); recomputeMakeup(); publish(); }
void ZamikProcessor::setBalanceAuto (bool b)  { cur.balAuto = b; recomputeMakeup(); publish(); }

void ZamikProcessor::pickSlot (int i)
{
    if (i < 0 || i >= kNumSlots || i == active) return;
    slots[(size_t) active] = cur;
    if (! slots[(size_t) i])
        slots[(size_t) i] = cur;              // an empty variation starts as a copy of the current one
    cur = *slots[(size_t) i];
    active = i;
    recomputeMakeup();
    publish();
}

void ZamikProcessor::setBypass (bool b)
{
    bypassParam->beginChangeGesture();
    bypassParam->setValueNotifyingHost (b ? 1.0f : 0.0f);
    bypassParam->endChangeGesture();
}

void ZamikProcessor::parameterValueChanged (int, float) { changed(); }

//==============================================================================
void ZamikProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree root (idState);
    root.setProperty (idActive, active, nullptr);
    root.setProperty (idBypass, bypassParam->get(), nullptr);
    juce::ValueTree c (idCurrent);
    c.appendChild (cur.toTree(), nullptr);
    root.appendChild (c, nullptr);
    for (int i = 0; i < kNumSlots; ++i)
        if (slots[(size_t) i] && i != active)
        {
            juce::ValueTree s (idSlot);
            s.setProperty (idIndex, i, nullptr);
            s.appendChild (slots[(size_t) i]->toTree(), nullptr);
            root.appendChild (s, nullptr);
        }
    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, dest);
}

void ZamikProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType (idState)) return;

    for (auto& s : slots) s.reset();
    active = juce::jlimit (0, kNumSlots - 1, (int) root.getProperty (idActive, 0));
    cur = Settings::fromTree (root.getChildWithName (idCurrent).getChild (0));
    slots[(size_t) active] = cur;
    for (auto s : root)
        if (s.hasType (idSlot))
        {
            const int i = (int) s.getProperty (idIndex, -1);
            if (i >= 0 && i < kNumSlots && i != active)
                slots[(size_t) i] = Settings::fromTree (s.getChild (0));
        }
    *bypassParam = (bool) root.getProperty (idBypass, false);
    recomputeMakeup();
    publish();
}

//==============================================================================
juce::AudioProcessorEditor* ZamikProcessor::createEditor() { return new ZamikEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ZamikProcessor(); }
