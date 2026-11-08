#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace zamik::ui
{
    namespace colours
    {
        const juce::Colour room      { 0xff121110 };
        const juce::Colour plate     { 0xff1c1a18 };
        const juce::Colour plateTop  { 0xff211e1b };
        const juce::Colour plateHi   { 0xff25221f };
        const juce::Colour well      { 0xff171513 };
        const juce::Colour line      { 0xff36312c };
        const juce::Colour lineBold  { 0xff4a433c };
        const juce::Colour ink       { 0xffddd6cc };
        const juce::Colour inkDim    { 0xff8e867b };
        const juce::Colour amber     { 0xffff9a3c };
        const juce::Colour amberDeep { 0xff3a2617 };
        const juce::Colour amberEdge { 0xff8a4f22 };
        const juce::Colour caption   { 0xffb07a4a };
        const juce::Colour ok        { 0xff8fd18a };
        const juce::Colour warn      { 0xffff5a43 };
        const juce::Colour matrix    { 0xffffb347 };
    }

    /** Embedded faces: Barlow Condensed for labels, JetBrains Mono for numbers. */
    struct Fonts
    {
        static juce::Font label (float size, bool bold = true, float kerning = 0.16f);
        static juce::Font data (float size, bool medium = false);
    };

    /** Draws the faceplate controls. A button's "zstyle" property picks the look:
        chip, chipLabel, btn, seg, bypass. */
    class ZamikLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        ZamikLookAndFeel();

        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
        void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
        juce::Font getTextButtonFont (juce::TextButton&, int) override;

        void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
        juce::Font getComboBoxFont (juce::ComboBox&) override;
        void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
        juce::Font getPopupMenuFont() override;
        void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;

        void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                               juce::Slider::SliderStyle, juce::Slider&) override;

        void drawTooltip (juce::Graphics&, const juce::String&, int w, int h) override;
        juce::Rectangle<int> getTooltipBounds (const juce::String&, juce::Point<int>, juce::Rectangle<int>) override;
    };
}
