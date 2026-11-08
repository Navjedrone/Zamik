#include "ZamikLookAndFeel.h"
#include "BinaryData.h"

namespace zamik::ui
{
    using namespace colours;

    namespace
    {
        juce::Typeface::Ptr load (const char* data, int size)
        {
            return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
        }

        juce::Typeface::Ptr labelMedium() { static auto t = load (BinaryData::BarlowCondensedMedium_ttf,   BinaryData::BarlowCondensedMedium_ttfSize);   return t; }
        juce::Typeface::Ptr labelBold()   { static auto t = load (BinaryData::BarlowCondensedSemiBold_ttf, BinaryData::BarlowCondensedSemiBold_ttfSize); return t; }
        juce::Typeface::Ptr dataRegular() { static auto t = load (BinaryData::JetBrainsMonoRegular_ttf,    BinaryData::JetBrainsMonoRegular_ttfSize);    return t; }
        juce::Typeface::Ptr dataMedium()  { static auto t = load (BinaryData::JetBrainsMonoMedium_ttf,     BinaryData::JetBrainsMonoMedium_ttfSize);     return t; }

        juce::String style (const juce::Component& c) { return c.getProperties()["zstyle"].toString(); }
    }

    juce::Font Fonts::label (float size, bool bold, float kerning)
    {
        return juce::Font (juce::FontOptions (bold ? labelBold() : labelMedium()).withHeight (size).withKerningFactor (kerning));
    }

    juce::Font Fonts::data (float size, bool medium)
    {
        return juce::Font (juce::FontOptions (medium ? dataMedium() : dataRegular()).withHeight (size));
    }

    ZamikLookAndFeel::ZamikLookAndFeel()
    {
        setColour (juce::PopupMenu::backgroundColourId, well);
        setColour (juce::PopupMenu::textColourId, ink);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, amberDeep);
        setColour (juce::PopupMenu::highlightedTextColourId, amber);
        setColour (juce::ComboBox::textColourId, ink);
        setColour (juce::TooltipWindow::textColourId, ink);
    }

    void ZamikLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
    {
        const auto st = style (b);
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        const float alpha = b.isEnabled() ? 1.0f : 0.35f;

        if (st == "seg")
        {
            const bool l = ! b.isConnectedOnLeft(), rr = ! b.isConnectedOnRight();
            juce::Path p;
            p.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(), 8.0f, 8.0f, l, rr, l, rr);
            g.setColour ((on ? amberDeep : well).withMultipliedAlpha (alpha));
            g.fillPath (p);
            g.setColour (line.withMultipliedAlpha (alpha));
            g.strokePath (p, juce::PathStrokeType (1.0f));
            if ((bool) b.getProperties()["used"])      // a stored variation
            {
                g.setColour (inkDim);
                g.fillEllipse (r.getRight() - 8.0f, r.getY() + 5.0f, 3.0f, 3.0f);
            }
            return;
        }

        const bool bypass = st == "bypass";
        const float corner = 6.0f, stroke = bypass ? 2.0f : 1.0f;
        r = r.reduced (stroke * 0.5f - 0.5f);
        auto fill = on ? amberDeep : plateHi;
        if (! on && (over || down)) fill = fill.brighter (0.05f);
        g.setColour (fill.withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (r, corner);
        auto edge = bypass ? (on ? amber : lineBold) : (on ? amberEdge : line);
        g.setColour (edge.withMultipliedAlpha (alpha));
        g.drawRoundedRectangle (r, corner, stroke);
    }

    juce::Font ZamikLookAndFeel::getTextButtonFont (juce::TextButton& b, int)
    {
        const auto st = style (b);
        if (st == "chip" || st == "bypass") return Fonts::data (12.5f);
        if (st == "chipLabel")              return Fonts::label (14.0f, false, 0.14f);
        if (st == "seg")                    return Fonts::label (15.0f, true, 0.14f);
        return Fonts::label (14.5f, true, 0.18f);
    }

    void ZamikLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool over, bool)
    {
        const bool on = b.getToggleState();
        auto c = on ? amber : (over ? ink : (style (b) == "btn" ? ink : inkDim));
        if (! b.isEnabled()) c = c.withMultipliedAlpha (0.4f);
        g.setColour (c);
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1, 1.0f);
    }

    void ZamikLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (well);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (box.hasKeyboardFocus (true) ? amberEdge : line);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
        juce::Path arrow;
        const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
        arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
        g.setColour (inkDim);
        g.fillPath (arrow);
    }

    juce::Font ZamikLookAndFeel::getComboBoxFont (juce::ComboBox&) { return Fonts::data (12.5f); }
    juce::Font ZamikLookAndFeel::getPopupMenuFont()                { return Fonts::data (13.0f); }

    void ZamikLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
    {
        label.setBounds (6, 1, box.getWidth() - 26, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    void ZamikLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
    {
        g.fillAll (well);
        g.setColour (line);
        g.drawRect (0, 0, w, h);
    }

    void ZamikLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                             juce::Slider::SliderStyle, juce::Slider& s)
    {
        const float cy = (float) y + (float) h * 0.5f;
        const float centre = (float) s.valueToProportionOfLength (0.0) * (float) w + (float) x;
        g.setColour (line);
        g.fillRoundedRectangle ((float) x, cy - 2.0f, (float) w, 4.0f, 2.0f);
        g.setColour (amber);
        g.fillRoundedRectangle (juce::jmin (centre, pos), cy - 2.0f, std::abs (pos - centre), 4.0f, 2.0f);
        g.setColour (inkDim.withAlpha (0.6f));
        g.fillRect (centre - 0.5f, cy - 6.0f, 1.0f, 12.0f);
        g.setColour (amber);
        g.fillEllipse (pos - 8.0f, cy - 8.0f, 16.0f, 16.0f);
        g.setColour (juce::Colour (0xffffe0b8));
        g.fillEllipse (pos - 3.0f, cy - 3.0f, 6.0f, 6.0f);
    }

    void ZamikLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
    {
        g.fillAll (well);
        g.setColour (line);
        g.drawRect (0, 0, w, h);
        g.setColour (ink);
        g.setFont (Fonts::data (12.0f));
        g.drawFittedText (text, juce::Rectangle<int> (w, h).reduced (8, 4), juce::Justification::centredLeft, 6);
    }

    juce::Rectangle<int> ZamikLookAndFeel::getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent)
    {
        juce::StringArray lines;
        lines.addLines (text);
        int wMax = 0;
        for (auto& l : lines)
            wMax = juce::jmax (wMax, (int) juce::GlyphArrangement::getStringWidth (Fonts::data (12.0f), l));
        const int w = juce::jmin (420, wMax + 20), hgt = lines.size() * 16 + 10;
        return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 12,
                                     pos.y > parent.getCentreY() ? pos.y - (hgt + 8) : pos.y + 16, w, hgt)
                   .constrainedWithin (parent);
    }
}
