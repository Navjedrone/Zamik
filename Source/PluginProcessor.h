#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Core.h"
#include <array>
#include <optional>

/**
    Zamik moves audio earlier or later by whole samples.

    The audio path is a ring buffer: each input sample is stored, and the output reads the
    sample stored D samples ago, where D = reported latency + offset. Samples are copied,
    never computed, so the output is bit-identical to the input, only moved in time.

    The one exception is DUAL + M/S with unequal mid/side offsets or a balance other than
    center: that needs a sum/difference matrix, which is arithmetic. The editor shows that
    state with the M/S MATRIX lamp.
*/
class ZamikProcessor : public juce::AudioProcessor,
                       private juce::AudioProcessorParameter::Listener
{
public:
    ZamikProcessor();
    ~ZamikProcessor() override;

    //== AudioProcessor ======================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    bool supportsDoublePrecisionProcessing() const override { return true; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Zamik"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return zamik::kMaxAdvanceSec + zamik::kMaxLaterSec; }

    int getNumPrograms() override    { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    //== Controls (message thread) ===========================================
    const zamik::Settings& settings() const { return cur; }
    int  activeSlot() const              { return active; }
    bool slotUsed (int i) const          { return slots[(size_t) i].has_value(); }
    bool isBypassed() const              { return bypassParam->get(); }

    void nudge (int dir, int lane);
    void setMode (zamik::Mode);
    void zero();
    void snap();
    bool canSnap() const;
    void setDual (bool);
    void setChMode (zamik::ChMode);
    void setSync (bool);
    void setMsStep (int);
    void setSmpStep (int);
    void setDiv (int);
    void setFeel (int);
    void setAdvance (int index);
    void setBalance (int);
    void setBalanceAuto (bool);
    void pickSlot (int);
    void setBypass (bool);

    //== Readouts (message thread) ===========================================
    zamik::HostTime hostTime() const;
    double sampleRate() const       { return sr.load(); }
    int latency() const             { return latencyNow.load(); }
    zamik::Applied appliedFor (int lane) const;
    bool matrixActive() const;
    float makeupDb() const          { return makeupDbNow; }

    /** Smallest advance setting that would remove the current limit, or -1. */
    int suggestedAdvance() const;

    /** Bumped whenever settings change, so the editor knows to refresh. */
    std::atomic<int> changeCount { 0 };

private:
    template <typename T> void process (juce::AudioBuffer<T>&, bool forceBypass);
    template <typename T> struct Rings
    {
        juce::AudioBuffer<T> io;   // one ring per bus channel
        juce::AudioBuffer<T> ms;   // mid and side rings
        juce::AudioBuffer<T> scratch; // two chunks of working space for mid/side
    };
    template <typename T> Rings<T>& rings();

    void publish();                 // push settings to the audio thread
    void recomputeMakeup();
    void parameterValueChanged (int, float) override;
    void parameterGestureChanged (int, bool) override {}
    void changed() { ++changeCount; }

    zamik::Settings cur;
    std::array<std::optional<zamik::Settings>, zamik::kNumSlots> slots;
    int active = 0;
    float makeupDbNow = 0.0f;

    juce::AudioParameterBool* bypassParam = nullptr;

    // message thread -> audio thread
    std::atomic<int>    aMode { 0 }, aDual { 0 }, aChm { 0 }, aBalOff { 0 };
    std::atomic<double> aMs[2], aBase[2];
    std::atomic<int>    aTicks[2];
    std::atomic<float>  aGm { 1.0f }, aGs { 1.0f };
    std::atomic<int>    latencyNow { 0 };

    // audio thread -> message thread
    std::atomic<double> sr { 48000.0 }, aBpm { 120.0 };
    std::atomic<int>    aNum { 4 }, aDen { 4 };
    std::atomic<double> energyM { 0.0 }, energyS { 0.0 };

    // audio thread only
    Rings<float>  ringsF;
    Rings<double> ringsD;
    int ringLen = 0, writePos = 0;
    double eM = 0.0, eS = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZamikProcessor)
};
