#include "NixieDisplay.h"
#include "ZamikLookAndFeel.h"

namespace zamik::ui
{
    using namespace colours;

    namespace
    {
        constexpr float kPad = 14.0f, kCaptionBlock = 42.0f, kRowGap = 8.0f, kTagW = 22.0f;
        constexpr float kTubeAspect = 120.0f / 62.0f;     // height / width

        float rad (float deg) { return juce::degreesToRadians (deg); }

        // Wire cathodes in a 0.6 x 1 box. JUCE arc angles start at 12 o'clock and run clockwise.
        void arc (juce::Path& p, float cx, float cy, float r, float a0, float a1)
        {
            p.addCentredArc (cx, cy, r, r, 0.0f, rad (a0), rad (a1), p.isEmpty());
        }

        juce::Path glyph (juce::juce_wchar c)
        {
            juce::Path p;
            switch (c)
            {
                case '0': p.addEllipse (0.05f, 0.05f, 0.5f, 0.9f); break;
                case '1': p.startNewSubPath (0.15f, 0.2f); p.lineTo (0.32f, 0.05f); p.lineTo (0.32f, 0.95f); break;
                case '2': arc (p, 0.3f, 0.28f, 0.23f, -80.0f, 128.0f); p.lineTo (0.05f, 0.95f); p.lineTo (0.57f, 0.95f); break;
                case '3': arc (p, 0.3f, 0.27f, 0.21f, -70.0f, 180.0f); arc (p, 0.3f, 0.725f, 0.245f, 0.0f, 245.0f); break;
                case '4': p.startNewSubPath (0.46f, 0.95f); p.lineTo (0.46f, 0.05f); p.lineTo (0.04f, 0.68f); p.lineTo (0.58f, 0.68f); break;
                case '5': p.startNewSubPath (0.54f, 0.05f); p.lineTo (0.13f, 0.05f); p.lineTo (0.115f, 0.5f); arc (p, 0.3f, 0.69f, 0.26f, -45.0f, 225.0f); break;
                case '6': p.startNewSubPath (0.5f, 0.06f); p.quadraticTo (0.06f, 0.25f, 0.05f, 0.7f); p.addEllipse (0.05f, 0.45f, 0.5f, 0.5f); break;
                case '7': p.startNewSubPath (0.04f, 0.05f); p.lineTo (0.56f, 0.05f); p.lineTo (0.2f, 0.95f); break;
                case '8': p.addEllipse (0.1f, 0.07f, 0.4f, 0.4f); p.addEllipse (0.07f, 0.49f, 0.46f, 0.46f); break;
                case '9': p = glyph ('6'); p.applyTransform (juce::AffineTransform::rotation (juce::MathConstants<float>::pi, 0.3f, 0.5f)); break;
                case '+': p.startNewSubPath (0.08f, 0.5f); p.lineTo (0.52f, 0.5f); p.startNewSubPath (0.3f, 0.28f); p.lineTo (0.3f, 0.72f); break;
                case '-': p.startNewSubPath (0.08f, 0.5f); p.lineTo (0.52f, 0.5f); break;
                case '/': p.startNewSubPath (0.54f, 0.06f); p.lineTo (0.06f, 0.94f); break;
                default: break;
            }
            return p;
        }

        juce::Path tubeOutline (juce::Rectangle<float> r)
        {
            const float x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();
            const float rt = w * 0.46f, rb = 6.0f;
            juce::Path p;
            p.startNewSubPath (x, y + h - rb);
            p.lineTo (x, y + rt);
            p.quadraticTo (x, y, x + w * 0.5f, y);
            p.quadraticTo (x + w, y, x + w, y + rt);
            p.lineTo (x + w, y + h - rb);
            p.quadraticTo (x + w, y + h, x + w - rb, y + h);
            p.lineTo (x + rb, y + h);
            p.quadraticTo (x, y + h, x, y + h - rb);
            p.closeSubPath();
            return p;
        }
    }

    int NixieDisplay::heightFor (int numRows, int width)
    {
        const float inner = (float) width - 2.0f * kPad - (numRows > 1 ? kTagW : 0.0f);
        const float tubeW = inner / (9.0f + 8.0f * 0.11f);
        const float rowH = juce::jmin (124.0f, tubeW * kTubeAspect);
        return (int) std::ceil (kPad + rowH * (float) numRows + kRowGap * (float) (numRows - 1) + 10.0f + kCaptionBlock);
    }

    void NixieDisplay::setRows (std::vector<std::vector<Tube>> r, juce::StringArray t) { rows = std::move (r); tags = std::move (t); repaint(); }
    void NixieDisplay::setCaption (const juce::String& c)                               { if (c != caption) { caption = c; repaint(); } }
    void NixieDisplay::setDimmed (bool d)                                               { if (d != dimmed) { dimmed = d; repaint(); } }

    void NixieDisplay::setLamps (bool tr, bool mxVis, bool mxOn, bool lim, const juce::String& tip, bool clickable)
    {
        transparent = tr; matrixVisible = mxVis; matrixOn = mxOn; limit = lim; limitTip = tip; limitClickable = clickable;
        repaint();
    }

    void NixieDisplay::paint (juce::Graphics& g)
    {
        auto b = getLocalBounds().toFloat();

        // bezel
        g.setColour (juce::Colour (0xff3a342e));
        g.fillRoundedRectangle (b, 11.0f);
        b = b.reduced (1.0f);
        g.setColour (juce::Colour (0xff060505));
        g.fillRoundedRectangle (b, 10.0f);
        {
            juce::ColourGradient shade (juce::Colours::black.withAlpha (0.7f), 0, b.getY(), juce::Colours::transparentBlack, 0, b.getY() + 16.0f, false);
            g.setGradientFill (shade);
            g.fillRoundedRectangle (b, 10.0f);
        }

        auto inner = b.reduced (kPad - 1.0f, 0).withTrimmedTop (kPad - 1.0f);
        auto captionArea = inner.removeFromBottom (kCaptionBlock).withTrimmedBottom (6.0f);
        inner.removeFromBottom (10.0f);

        const int numRows = (int) rows.size();
        const float rowH = numRows > 0 ? (inner.getHeight() - kRowGap * (float) (numRows - 1)) / (float) numRows : 0.0f;

        for (int ri = 0; ri < numRows; ++ri)
        {
            auto row = inner.removeFromTop (rowH);
            inner.removeFromTop (kRowGap);
            if (numRows > 1)
            {
                auto tagArea = row.removeFromLeft (kTagW);
                g.setColour (caption.isEmpty() ? colours::caption : colours::caption);
                g.setFont (Fonts::label (17.0f, true, 0.0f));
                g.drawText (tags[ri], tagArea, juce::Justification::centredLeft);
            }
            const auto& tubes = rows[(size_t) ri];
            const int n = (int) tubes.size();
            float tubeW = row.getWidth() / ((float) n + (float) (n - 1) * 0.11f);
            float tubeH = tubeW * kTubeAspect;
            if (tubeH > row.getHeight()) { tubeH = row.getHeight(); tubeW = tubeH / kTubeAspect; }
            const float gap = tubeW * 0.11f;
            const float total = tubeW * (float) n + gap * (float) (n - 1);
            float x = row.getX() + (row.getWidth() - total) * 0.5f;
            const float y = row.getY() + (row.getHeight() - tubeH) * 0.5f;

            juce::Graphics::ScopedSaveState ss (g);
            if (dimmed) g.setOpacity (0.28f);
            for (auto& t : tubes)
            {
                drawTube (g, { x, y, tubeW, tubeH }, t);
                x += tubeW + gap;
            }
        }

        // caption + lamps
        auto capLine = captionArea.removeFromTop (18.0f);
        g.setColour (colours::caption);
        g.setFont (Fonts::data (11.5f));
        g.drawFittedText (caption, capLine.toNearestInt(), juce::Justification::centredLeft, 1, 0.85f);

        auto lampLine = captionArea.withTrimmedTop (4.0f);
        drawLamp (g, lampLine, "BIT-TRANSPARENT", transparent, ok);
        if (matrixVisible)
            drawLamp (g, lampLine, "M/S MATRIX", matrixOn, matrix);
        drawLamp (g, lampLine, "LIMIT", limit, warn, &limitHit);
    }

    void NixieDisplay::drawLamp (juce::Graphics& g, juce::Rectangle<float>& area, const juce::String& text, bool on,
                                 juce::Colour onColour, juce::Rectangle<float>* hit) const
    {
        const auto font = Fonts::data (11.5f);
        const float tw = juce::GlyphArrangement::getStringWidth (font, text);
        auto r = area.removeFromLeft (tw + 14.0f);
        area.removeFromLeft (16.0f);
        if (hit) *hit = r.expanded (4.0f, 3.0f);

        const auto dot = juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ r.getX() + 3.5f, r.getCentreY() });
        if (on)
        {
            g.setColour (onColour.withAlpha (0.35f));
            g.fillEllipse (dot.expanded (3.5f));
        }
        g.setColour (on ? onColour : juce::Colour (0xff3a2218));
        g.fillEllipse (dot);
        g.setColour (on ? onColour.interpolatedWith (juce::Colours::white, 0.15f) : juce::Colour (0xff5b4636));
        g.setFont (font);
        g.drawText (text, r.withTrimmedLeft (14.0f), juce::Justification::centredLeft);
    }

    void NixieDisplay::drawTube (juce::Graphics& g, juce::Rectangle<float> r, const Tube& t) const
    {
        const auto env = tubeOutline (r);
        {
            juce::ColourGradient glass (juce::Colour (0xff1d1714), 0, r.getY(), juce::Colour (0xff0b0908), 0, r.getBottom(), false);
            g.setGradientFill (glass);
            g.fillPath (env);
        }

        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (env);

        const bool glowing = (t.lit && t.ch != ' ') || t.dp;
        if (glowing)
        {
            juce::ColourGradient ambient (juce::Colour (0x33ff7828), r.getCentreX(), r.getY() + r.getHeight() * 0.52f,
                                          juce::Colour (0x00ff7828), r.getCentreX() + r.getWidth() * 0.9f, r.getY() + r.getHeight() * 0.52f, true);
            g.setGradientFill (ambient);
            g.fillRect (r);
        }

        // anode mesh
        g.setColour (juce::Colour (0x0fa08c78));
        for (float k = -r.getHeight(); k < r.getWidth() + r.getHeight(); k += 5.0f)
        {
            g.drawLine (r.getX() + k, r.getY(), r.getX() + k + r.getHeight() * 0.6f, r.getBottom(), 1.0f);
            g.drawLine (r.getX() + k, r.getY(), r.getX() + k - r.getHeight() * 0.6f, r.getBottom(), 1.0f);
        }

        // glyph box
        const float s = r.getHeight() * 0.62f;
        const auto xf = juce::AffineTransform::scale (s).translated (r.getX() + (r.getWidth() - 0.6f * s) * 0.5f, r.getY() + r.getHeight() * 0.17f);
        const juce::PathStrokeType thin (1.1f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        for (auto ghost : { '8', '0', '2', '5' })    // unlit stacked cathodes
        {
            auto p = glyph ((juce::juce_wchar) ghost);
            p.applyTransform (xf);
            g.setColour (juce::Colour (0x21786c5c));
            g.strokePath (p, thin);
        }

        if (t.lit && t.ch != ' ')
        {
            auto p = glyph (t.ch);
            p.applyTransform (xf);
            const float k = juce::jlimit (0.6f, 1.2f, r.getWidth() / 58.0f);
            const std::pair<float, float> layers[] = { { 16.0f, 0.04f }, { 11.0f, 0.07f }, { 7.0f, 0.13f }, { 4.6f, 0.30f } };
            for (auto [w, a] : layers)
            {
                g.setColour (juce::Colour (0xffff6e1e).withAlpha (a));
                g.strokePath (p, juce::PathStrokeType (w * k, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            g.setColour (juce::Colour (0xffff8a30));
            g.strokePath (p, juce::PathStrokeType (2.6f * k, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (juce::Colour (0xffffe0b8));
            g.strokePath (p, juce::PathStrokeType (1.1f * k, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // decimal point cathode
        const auto dot = juce::Rectangle<float> (4.8f, 4.8f).withCentre ({ r.getX() + r.getWidth() * 0.84f, r.getY() + r.getHeight() * 0.79f });
        if (t.dp)
        {
            g.setColour (juce::Colour (0x55ff7a20));
            g.fillEllipse (dot.expanded (4.0f));
            g.setColour (juce::Colour (0xffffcf98));
        }
        else
            g.setColour (juce::Colour (0x2e786858));
        g.fillEllipse (dot);

        // glass reflection and base
        juce::ColourGradient refl (juce::Colours::white.withAlpha (0.10f), r.getX(), 0, juce::Colours::white.withAlpha (0.0f), r.getX() + r.getWidth() * 0.35f, 0, false);
        g.setGradientFill (refl);
        g.fillRect (r.getX() + r.getWidth() * 0.08f, r.getY() + r.getHeight() * 0.08f, r.getWidth() * 0.22f, r.getHeight() * 0.8f);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRect (r.getX(), r.getBottom() - 9.0f, r.getWidth(), 9.0f);

        g.setColour (juce::Colour (0x1ffff0dc));
        g.strokePath (env, juce::PathStrokeType (1.0f));
    }

    void NixieDisplay::mouseDown (const juce::MouseEvent& e)
    {
        if (limitClickable && limitHit.contains (e.position) && onLimitClicked)
            onLimitClicked();
    }

    void NixieDisplay::mouseMove (const juce::MouseEvent& e)
    {
        setMouseCursor (limitClickable && limitHit.contains (e.position) ? juce::MouseCursor::PointingHandCursor
                                                                         : juce::MouseCursor::NormalCursor);
    }

    juce::String NixieDisplay::getTooltip()
    {
        return limitHit.contains (getMouseXYRelative().toFloat()) ? limitTip : juce::String();
    }
}
