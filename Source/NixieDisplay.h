#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace zamik::ui
{
    struct Tube
    {
        juce::juce_wchar ch = ' ';
        bool lit = false;
        bool dp  = false;     // decimal point lit
    };

    /** The black bezel: one or two rows of nixie tubes, a caption line and status lamps. */
    class NixieDisplay : public juce::Component,
                         public juce::TooltipClient
    {
    public:
        void setRows (std::vector<std::vector<Tube>> rows, juce::StringArray tags);
        void setCaption (const juce::String&);
        void setLamps (bool transparent, bool matrixVisible, bool matrixOn, bool limit, const juce::String& limitTip, bool limitClickable);
        void setDimmed (bool);

        std::function<void()> onLimitClicked;

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;
        juce::String getTooltip() override;

        static int heightFor (int rows, int width);

    private:
        void drawTube (juce::Graphics&, juce::Rectangle<float>, const Tube&) const;
        void drawLamp (juce::Graphics&, juce::Rectangle<float>&, const juce::String&, bool on, juce::Colour onColour, juce::Rectangle<float>* hit = nullptr) const;

        std::vector<std::vector<Tube>> rows;
        juce::StringArray tags;
        juce::String caption, limitTip;
        bool transparent = true, matrixVisible = false, matrixOn = false, limit = false, limitClickable = false, dimmed = false;
        juce::Rectangle<float> limitHit;
    };
}
