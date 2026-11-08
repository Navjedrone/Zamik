#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ZamikLookAndFeel.h"
#include "NixieDisplay.h"

namespace zamik::ui
{
    /** Big hardware-style arrow key. Holding it repeats. */
    class ArrowButton : public juce::Button
    {
    public:
        ArrowButton (bool pointsLeft, juce::String label);
        void setLabel (const juce::String& l) { if (l != label) { label = l; repaint(); } }
        void paintButton (juce::Graphics&, bool over, bool down) override;
    private:
        bool left;
        juce::String label;
    };

    /** Mid/side balance pop-up. */
    class BalancePanel : public juce::Component
    {
    public:
        explicit BalancePanel (ZamikProcessor&);
        void refresh();
        void paint (juce::Graphics&) override;
        void resized() override;
        std::function<void()> onClose;
    private:
        ZamikProcessor& proc;
        juce::Slider slider;
        juce::TextButton centreBtn { "CENTER" }, autoBtn { "AUTO GAIN" }, closeBtn { juce::String::charToString (0x00d7) };
        juce::String valueText, gainText, makeupText;
    };
}

class ZamikEditor : public juce::AudioProcessorEditor,
                    private juce::Timer
{
public:
    explicit ZamikEditor (ZamikProcessor&);
    ~ZamikEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void refresh();
    void layoutFor (bool dual);
    void toggleBalance();
    juce::String captionText() const;
    juce::String sampleRateText() const;

    ZamikProcessor& proc;
    zamik::ui::ZamikLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };
    std::unique_ptr<juce::Drawable> logo;

    juce::TextButton varBtn[zamik::kNumSlots], modeBtn[3];
    zamik::ui::NixieDisplay display;
    zamik::ui::ArrowButton bigEarly { true, "EARLIER" }, bigLate { false, "LATER" };
    zamik::ui::ArrowButton laneEarly[2] { { true, "L" }, { true, "R" } }, laneLate[2] { { false, "L" }, { false, "R" } };

    juce::TextButton msChips[zamik::kNumSteps], smpChips[zamik::kNumSteps], divChips[zamik::kNumDivs], feelChips[3];
    juce::TextButton zeroBtn { "ZERO" }, snapBtn { "SNAP" }, dualBtn { "DUAL" }, chLR { "L/R" }, chMS { "M/S" },
                     balBtn { "BAL" }, syncBtn { "SYNC" }, bypassBtn { "BYPASS" };
    juce::ComboBox advanceBox;
    zamik::ui::BalancePanel balPanel;

    juce::Rectangle<int> plateArea, centreArea, footLine, advanceLabelArea;
    juce::String centreLabel, centreValue;
    bool laidOutDual = false;
    juce::String lastDisplayKey, lastPlateKey;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZamikEditor)
};
