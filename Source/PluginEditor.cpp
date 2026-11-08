#include "PluginEditor.h"
#include "BinaryData.h"
#include <numeric>

using namespace zamik;
using namespace zamik::ui;
using namespace zamik::ui::colours;

namespace
{
    constexpr int kW = 720, kMargin = 20;

    void styleAs (juce::Component& c, const char* st) { c.getProperties().set ("zstyle", st); }

    juce::String fmtMs (double ms) { return (ms > 0 ? "+" : "") + juce::String (ms, 3); }

    std::vector<Tube> msTubes (double ms)
    {
        std::vector<Tube> out;
        out.push_back ({ ms > 0 ? '+' : ms < 0 ? '-' : ' ', std::abs (ms) >= 0.0005 });
        auto txt = juce::String (std::abs (ms), 3);
        auto ip = txt.upToFirstOccurrenceOf (".", false, false).paddedLeft ('0', 5);
        auto fp = txt.fromFirstOccurrenceOf (".", false, false).paddedRight ('0', 3).substring (0, 3);
        bool lead = true;
        for (int i = 0; i < 5; ++i)
        {
            if (ip[i] != '0' || i == 4) lead = false;
            out.push_back ({ ip[i], ! lead, i == 4 });
        }
        for (int i = 0; i < 3; ++i) out.push_back ({ fp[i], true });
        return out;
    }

    std::vector<Tube> smpTubes (juce::int64 v)
    {
        std::vector<Tube> out;
        out.push_back ({ v > 0 ? '+' : v < 0 ? '-' : ' ', v != 0 });
        auto s = juce::String (std::abs (v)).paddedLeft ('0', 8).getLastCharacters (8);
        bool lead = true;
        for (int i = 0; i < 8; ++i)
        {
            if (s[i] != '0' || i == 7) lead = false;
            out.push_back ({ s[i], ! lead });
        }
        return out;
    }

    std::vector<Tube> noteTubes (int ticks)
    {
        std::vector<Tube> out;
        out.push_back ({ ticks > 0 ? '+' : ticks < 0 ? '-' : ' ', ticks != 0 });
        juce::String s ("0");
        if (ticks != 0)
        {
            const int a = std::abs (ticks), g = std::gcd (a, kTicksPerWhole);
            s = juce::String (a / g) + "/" + juce::String (kTicksPerWhole / g);
        }
        s = s.paddedLeft (' ', 8).getLastCharacters (8);
        for (int i = 0; i < 8; ++i) out.push_back ({ s[i], s[i] != ' ' });
        return out;
    }
}

//==============================================================================
ArrowButton::ArrowButton (bool pointsLeft, juce::String l) : juce::Button ({}), left (pointsLeft), label (std::move (l))
{
    setRepeatSpeed (420, 70);
    setWantsKeyboardFocus (false);
}

void ArrowButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const float alpha = isEnabled() ? 1.0f : 0.3f;
    const float press = down ? 2.0f : 0.0f;

    g.setColour (juce::Colour (0xff0e0d0c).withMultipliedAlpha (alpha));
    g.fillRoundedRectangle (r.withTrimmedTop (3.0f), 10.0f);
    auto face = r.withTrimmedBottom (3.0f).translated (0, press);
    juce::ColourGradient grad (down ? juce::Colour (0xff2a201a) : juce::Colour (0xff2b2724), 0, face.getY(),
                               down ? juce::Colour (0xff1f1915) : juce::Colour (0xff201d1a), 0, face.getBottom(), false);
    g.setGradientFill (grad);
    g.setOpacity (alpha);
    g.fillRoundedRectangle (face, 10.0f);
    g.setColour (line.withMultipliedAlpha (alpha));
    g.drawRoundedRectangle (face, 10.0f, 1.0f);

    const auto fg = (down ? amber : over ? ink.brighter (0.1f) : ink).withMultipliedAlpha (alpha);
    const bool small = getHeight() < 90;
    const float tri = small ? 11.0f : 16.0f;
    const float labelH = label.isEmpty() ? 0.0f : (small ? 14.0f : 18.0f);
    const float cx = face.getCentreX(), cy = face.getCentreY() - labelH * 0.5f;
    juce::Path p;
    if (left) p.addTriangle (cx + tri * 0.55f, cy - tri, cx + tri * 0.55f, cy + tri, cx - tri * 0.8f, cy);
    else      p.addTriangle (cx - tri * 0.55f, cy - tri, cx - tri * 0.55f, cy + tri, cx + tri * 0.8f, cy);
    g.setColour (fg);
    g.fillPath (p);

    if (labelH > 0)
    {
        g.setColour ((down ? amber : inkDim).withMultipliedAlpha (alpha));
        g.setFont (Fonts::label (small ? 13.0f : 14.0f, false, 0.2f));
        g.drawText (label, juce::Rectangle<float> (face.getX(), cy + tri + 6.0f, face.getWidth(), labelH), juce::Justification::centredTop);
    }
}

//==============================================================================
BalancePanel::BalancePanel (ZamikProcessor& p) : proc (p)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRange (-100.0, 100.0, 1.0);
    slider.setDoubleClickReturnValue (true, 0.0);
    slider.onValueChange = [this] { proc.setBalance ((int) slider.getValue()); refresh(); };
    addAndMakeVisible (slider);

    for (auto* b : { &centreBtn, &autoBtn }) { styleAs (*b, "btn"); addAndMakeVisible (*b); }
    styleAs (closeBtn, "btn");
    addAndMakeVisible (closeBtn);
    closeBtn.setTooltip ("Close");
    centreBtn.onClick = [this] { proc.setBalance (0); refresh(); };
    autoBtn.onClick   = [this] { proc.setBalanceAuto (! proc.settings().balAuto); refresh(); };
    closeBtn.onClick  = [this] { if (onClose) onClose(); };
    autoBtn.setTooltip ("Keep the overall level the same as the balance moves");
}

void BalancePanel::refresh()
{
    const auto& s = proc.settings();
    slider.setValue (s.bal, juce::dontSendNotification);
    autoBtn.setToggleState (s.balAuto, juce::dontSendNotification);
    valueText = s.bal == 0 ? "CENTER" : s.bal < 0 ? "MID " + juce::String (-s.bal) : "SIDE " + juce::String (s.bal);

    float gm, gs;
    balanceGains (s.bal, gm, gs);
    const float mkDb = (s.balAuto && s.bal != 0) ? proc.makeupDb() : 0.0f;
    const float mk = std::pow (10.0f, mkDb / 20.0f);
    auto db = [] (float g) -> juce::String
    {
        if (g <= 0.0f) return "-inf";
        const float v = 20.0f * std::log10 (g);
        return std::abs (v) < 0.05f ? "0.0" : (v > 0 ? "+" : "") + juce::String (v, 1);
    };
    gainText   = "MID " + db (gm * mk) + " dB  SIDE " + db (gs * mk) + " dB";
    makeupText = s.balAuto ? "MAKEUP " + db (mk) + " dB" + (mkDb > 11.95f ? " MAX" : "") : "OFF";
    repaint();
}

void BalancePanel::resized()
{
    auto r = getLocalBounds().reduced (14, 12);
    closeBtn.setBounds (r.getRight() - 24, r.getY(), 24, 24);
    slider.setBounds (r.getX() - 4, r.getY() + 46, r.getWidth() + 8, 24);
    centreBtn.setBounds (r.getRight() - 84, r.getY() + 78, 84, 28);
    autoBtn.setBounds (r.getX(), r.getY() + 114, 110, 28);
}

void BalancePanel::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (b.translated (0, 6), 10.0f);
    g.setColour (juce::Colour (0xff191715));
    g.fillRoundedRectangle (b, 10.0f);
    g.setColour (line);
    g.drawRoundedRectangle (b, 10.0f, 1.0f);

    auto r = getLocalBounds().reduced (14, 12);
    g.setFont (Fonts::label (13.5f, false, 0.2f));
    g.setColour (inkDim);
    g.drawText ("M/S BALANCE", r.getX(), r.getY(), 140, 24, juce::Justification::centredLeft);
    g.setColour (amber);
    g.setFont (Fonts::data (12.0f));
    g.drawText (valueText, r.getX(), r.getY(), r.getWidth() - 34, 24, juce::Justification::centredRight);

    g.setColour (inkDim);
    g.setFont (Fonts::label (13.0f, false, 0.2f));
    g.drawText ("MID", r.getX(), r.getY() + 28, 60, 16, juce::Justification::centredLeft);
    g.drawText ("SIDE", r.getRight() - 60, r.getY() + 28, 60, 16, juce::Justification::centredRight);

    g.setColour (ink);
    g.setFont (Fonts::data (11.5f));
    g.drawFittedText (gainText, r.getX(), r.getY() + 78, r.getWidth() - 92, 28, juce::Justification::centredLeft, 2);
    g.drawText (makeupText, r.getX() + 118, r.getY() + 114, r.getWidth() - 118, 28, juce::Justification::centredRight);

    g.setColour (juce::Colour (0xffc08a5c));
    g.setFont (Fonts::data (10.5f));
    g.drawFittedText ("Off center, mid and side are scaled, which changes sample values. Center is bit-transparent. "
                      "Auto gain is set when you move the slider, from the level of the last few seconds, and then stays fixed. "
                      "Double-click the slider to center it.",
                      r.getX(), r.getY() + 150, r.getWidth(), r.getBottom() - (r.getY() + 150), juce::Justification::topLeft, 6, 1.0f);
}

//==============================================================================
ZamikEditor::ZamikEditor (ZamikProcessor& p)
    : AudioProcessorEditor (p), proc (p), balPanel (p)
{
    setLookAndFeel (&lnf);
    setWantsKeyboardFocus (true);

    if (auto xml = juce::XmlDocument::parse (juce::String::fromUTF8 (BinaryData::logo_svg, BinaryData::logo_svgSize)))
        logo = juce::Drawable::createFromSVG (*xml);

    const char* varNames[] = { "A", "B", "C", "D" };
    for (int i = 0; i < kNumSlots; ++i)
    {
        auto& b = varBtn[i];
        b.setButtonText (varNames[i]);
        styleAs (b, "seg");
        b.setConnectedEdges ((i > 0 ? juce::Button::ConnectedOnLeft : 0) | (i < kNumSlots - 1 ? juce::Button::ConnectedOnRight : 0));
        b.onClick = [this, i] { proc.pickSlot (i); refresh(); };
        b.setTooltip ("Variation " + juce::String (varNames[i]) + ". An empty variation starts as a copy of the current one.");
        addAndMakeVisible (b);
    }

    const char* modeNames[] = { "MS", "SMP", "NOTE" };
    for (int i = 0; i < 3; ++i)
    {
        auto& b = modeBtn[i];
        b.setButtonText (modeNames[i]);
        styleAs (b, "seg");
        b.setConnectedEdges ((i > 0 ? juce::Button::ConnectedOnLeft : 0) | (i < 2 ? juce::Button::ConnectedOnRight : 0));
        b.onClick = [this, i] { proc.setMode ((Mode) i); refresh(); };
        addAndMakeVisible (b);
    }

    addAndMakeVisible (display);
    display.onLimitClicked = [this]
    {
        const int s = proc.suggestedAdvance();
        if (s >= 0) { proc.setAdvance (s); refresh(); }
    };

    bigEarly.onClick = [this] { proc.nudge (-1, 0); refresh(); };
    bigLate.onClick  = [this] { proc.nudge (+1, 0); refresh(); };
    addAndMakeVisible (bigEarly);
    addAndMakeVisible (bigLate);
    for (int i = 0; i < 2; ++i)
    {
        laneEarly[i].onClick = [this, i] { proc.nudge (-1, i); refresh(); };
        laneLate[i].onClick  = [this, i] { proc.nudge (+1, i); refresh(); };
        addChildComponent (laneEarly[i]);
        addChildComponent (laneLate[i]);
    }

    for (int i = 0; i < kNumSteps; ++i)
    {
        msChips[i].setButtonText (juce::String::fromUTF8 (kMsStepNames[i]));
        smpChips[i].setButtonText (juce::String (kSmpStep[i]));
        styleAs (msChips[i], "chip");
        styleAs (smpChips[i], "chip");
        msChips[i].onClick  = [this, i] { proc.setMsStep (i); refresh(); };
        smpChips[i].onClick = [this, i] { proc.setSmpStep (i); refresh(); };
        addChildComponent (msChips[i]);
        addChildComponent (smpChips[i]);
    }
    for (int i = 0; i < kNumDivs; ++i)
    {
        divChips[i].setButtonText (kDivNames[i]);
        styleAs (divChips[i], "chip");
        divChips[i].onClick = [this, i] { proc.setDiv (i); refresh(); };
        addChildComponent (divChips[i]);
    }
    for (int i = 0; i < 3; ++i)
    {
        feelChips[i].setButtonText (kFeelNames[i]);
        styleAs (feelChips[i], "chipLabel");
        feelChips[i].onClick = [this, i] { proc.setFeel (i); refresh(); };
        addChildComponent (feelChips[i]);
    }

    for (auto* b : { &zeroBtn, &snapBtn, &dualBtn, &balBtn, &syncBtn })
    {
        styleAs (*b, "btn");
        addAndMakeVisible (*b);
    }
    styleAs (chLR, "seg"); chLR.setConnectedEdges (juce::Button::ConnectedOnRight);
    styleAs (chMS, "seg"); chMS.setConnectedEdges (juce::Button::ConnectedOnLeft);
    addAndMakeVisible (chLR);
    addAndMakeVisible (chMS);
    styleAs (bypassBtn, "bypass");
    addAndMakeVisible (bypassBtn);

    zeroBtn.onClick   = [this] { proc.zero(); refresh(); };
    snapBtn.onClick   = [this] { proc.snap(); refresh(); };
    dualBtn.onClick   = [this] { proc.setDual (! proc.settings().dual); refresh(); };
    chLR.onClick      = [this] { proc.setChMode (ChMode::lr); refresh(); };
    chMS.onClick      = [this] { proc.setChMode (ChMode::ms); refresh(); };
    syncBtn.onClick   = [this] { proc.setSync (! proc.settings().sync); refresh(); };
    bypassBtn.onClick = [this] { proc.setBypass (! proc.isBypassed()); refresh(); };
    balBtn.onClick    = [this] { toggleBalance(); };

    zeroBtn.setTooltip ("Reset both offsets to zero");
    snapBtn.setTooltip ("Clear the ms/sample base and move to the nearest grid line of the selected division");
    syncBtn.setTooltip ("Lock the offset between the two displays; the upper display moves both");
    bypassBtn.setTooltip ("Audio passes unshifted and stays in time with other tracks");

    for (int i = 0; i < kNumAdvance; ++i)
        advanceBox.addItem (juce::String (kAdvanceMs[i]) + " ms", i + 1);
    advanceBox.onChange = [this] { proc.setAdvance (advanceBox.getSelectedId() - 1); refresh(); };
    advanceBox.setTooltip ("How far audio can move earlier. Zamik reports this much latency, which the host compensates.");
    addAndMakeVisible (advanceBox);

    addChildComponent (balPanel);
    balPanel.onClose = [this] { balPanel.setVisible (false); };

    addMouseListener (this, true);     // to close the balance pop-up on outside clicks

    laidOutDual = proc.settings().dual;
    layoutFor (laidOutDual);
    refresh();
    startTimerHz (30);
}

ZamikEditor::~ZamikEditor()
{
    removeMouseListener (this);
    setLookAndFeel (nullptr);
}

void ZamikEditor::layoutFor (bool dual)
{
    const int dispW  = dual ? kW - 2 * kMargin - 2 * 68 : kW - 2 * kMargin;
    const int dispH  = NixieDisplay::heightFor (dual ? 2 : 1, dispW);
    const int footH  = dual ? 30 + 10 + 30 : 30;
    const int height = 74 + dispH + 18 + 110 + 16 + 14 + footH + 20;
    if (getWidth() != kW || getHeight() != height)
        setSize (kW, height);
    else
        resized();
}

void ZamikEditor::resized()
{
    const bool dual = proc.settings().dual;
    plateArea = getLocalBounds();

    // header
    int hx = kMargin + 4;
    for (int i = 0; i < kNumSlots; ++i)
        varBtn[i].setBounds (hx + 186 + i * 36, 25, 36, 30);
    for (int i = 0; i < 3; ++i)
        modeBtn[i].setBounds (kW - kMargin - 4 - 3 * 66 + i * 66, 24, 66, 32);

    // display (+ lane arrows in DUAL)
    const int dispX = dual ? kMargin + 68 : kMargin;
    const int dispW = dual ? kW - 2 * kMargin - 2 * 68 : kW - 2 * kMargin;
    const int dispH = NixieDisplay::heightFor (dual ? 2 : 1, dispW);
    display.setBounds (dispX, 74, dispW, dispH);

    if (dual)
    {
        // line the lane arrows up with the two tube rows
        const int rowsTop = 74 + 13, rowsH = dispH - 13 - 10 - 42;
        const int rowH = (rowsH - 8) / 2;
        for (int i = 0; i < 2; ++i)
        {
            laneEarly[i].setBounds (kMargin, rowsTop + i * (rowH + 8), 58, rowH);
            laneLate[i].setBounds (kW - kMargin - 58, rowsTop + i * (rowH + 8), 58, rowH);
        }
    }

    // controls
    const int cy = 74 + dispH + 18;
    bigEarly.setBounds (kMargin, cy, 92, 110);
    bigLate.setBounds (kW - kMargin - 92, cy, 92, 110);
    centreArea = dual ? juce::Rectangle<int> (kMargin, cy + 7, kW - 2 * kMargin, 96)
                      : juce::Rectangle<int> (kMargin + 106, cy + 7, kW - 2 * kMargin - 212, 96);

    auto rowOf = [] (juce::Rectangle<int> r, juce::TextButton* buttons, int count, int gap)
    {
        const float w = (r.getWidth() - gap * (count - 1)) / (float) count;
        for (int i = 0; i < count; ++i)
            buttons[i].setBounds (juce::Rectangle<float> (r.getX() + i * (w + gap), (float) r.getY(), w, (float) r.getHeight()).toNearestInt());
    };
    rowOf ({ centreArea.getX(), centreArea.getY() + 30, centreArea.getWidth(), 36 }, msChips, kNumSteps, 6);
    rowOf ({ centreArea.getX(), centreArea.getY() + 30, centreArea.getWidth(), 36 }, smpChips, kNumSteps, 6);
    rowOf ({ centreArea.getX(), centreArea.getY() + 24, centreArea.getWidth(), 32 }, divChips, kNumDivs, 5);
    rowOf ({ centreArea.getX(), centreArea.getY() + 62, centreArea.getWidth(), 30 }, feelChips, 3, 6);

    // footer
    const int fy = cy + 110 + 16;
    footLine = { kMargin, fy, kW - 2 * kMargin, 1 };
    const int row1 = fy + 14;
    int x = kMargin;
    auto place = [&] (juce::Component& c, int w)
    {
        if (! c.isVisible()) return;
        c.setBounds (x, row1, w, 30);
        x += w + 8;
    };
    place (zeroBtn, 72);
    place (snapBtn, 66);
    place (dualBtn, 88);
    if (chLR.isVisible())
    {
        chLR.setBounds (x, row1, 50, 30);
        chMS.setBounds (x + 50, row1, 50, 30);
        x += 108;
    }
    place (balBtn, 92);
    place (syncBtn, 72);

    const int row2 = dual ? row1 + 40 : row1;
    bypassBtn.setBounds (kW - kMargin - 104, row2, 104, 30);
    advanceBox.setBounds (bypassBtn.getX() - 18 - 108, row2, 108, 30);
    advanceLabelArea = { advanceBox.getX() - 10 - 130, row2, 130, 30 };

    // balance pop-up above its button
    const int bw = 300, bh = 236;
    balPanel.setBounds (juce::jlimit (8, kW - bw - 8, balBtn.getX()), juce::jmax (8, balBtn.getY() - bh - 10), bw, bh);
}

void ZamikEditor::paint (juce::Graphics& g)
{
    g.fillAll (room);
    auto plate = getLocalBounds().toFloat().reduced (4.0f);
    juce::ColourGradient grad (plateTop, 0, plate.getY(), colours::plate, 0, plate.getY() + plate.getHeight() * 0.4f, false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (plate, 14.0f);
    g.setColour (line);
    g.drawRoundedRectangle (plate, 14.0f, 1.0f);

    for (float sx : { plate.getX() + 10.0f, plate.getRight() - 18.0f })     // rack screws
    {
        juce::ColourGradient screw (juce::Colour (0xff4a443d), sx + 3, plate.getY() + 13, juce::Colour (0xff1a1816), sx + 8, plate.getY() + 18, true);
        g.setGradientFill (screw);
        g.fillEllipse (sx, plate.getY() + 10.0f, 8.0f, 8.0f);
    }

    // brand
    if (logo)
        logo->drawWithin (g, { (float) kMargin + 4, 19.0f, 42.0f, 42.0f }, juce::RectanglePlacement::centred, 1.0f);
    g.setColour (ink);
    g.setFont (Fonts::label (27.0f, true, 0.32f));
    g.drawText ("ZAMIK", kMargin + 58, 22, 130, 36, juce::Justification::centredLeft);

    // centre labels
    g.setFont (Fonts::label (14.0f, false, 0.2f));
    g.setColour (inkDim);
    g.drawText (centreLabel, centreArea.getX(), centreArea.getY(), 200, 18, juce::Justification::centredLeft);
    g.setFont (Fonts::data (12.5f));
    g.setColour (ink);
    g.drawText (centreValue, centreArea.getRight() - 240, centreArea.getY(), 240, 18, juce::Justification::centredRight);

    // footer
    g.setColour (line);
    g.fillRect (footLine);
    g.setColour (inkDim);
    g.setFont (Fonts::label (14.0f, false, 0.2f));
    g.drawText ("ADVANCE RANGE", advanceLabelArea, juce::Justification::centredRight);
}

juce::String ZamikEditor::sampleRateText() const
{
    const double k = proc.sampleRate() / 1000.0;
    return (std::abs (k - std::round (k)) < 0.01 ? juce::String ((int) std::round (k)) : juce::String (k, 1)) + " kHz";
}

juce::String ZamikEditor::captionText() const
{
    const auto& s = proc.settings();
    const juce::String sepU (juce::CharPointer_UTF8 ("  \xc2\xb7  "));
    if (proc.isBypassed())
        return "BYPASSED" + sepU + "SETTINGS KEPT" + sepU + "AUDIO UNSHIFTED";
    const double rate = proc.sampleRate();
    const auto a = proc.appliedFor (0), b = proc.appliedFor (1);
    const double ms = (double) a.samples * 1000.0 / rate;
    const juce::String dir = a.samples > 0 ? "LATER" : a.samples < 0 ? "EARLIER" : "IN TIME";
    const juce::String tags[2] = { s.chm == ChMode::ms ? "M" : "L", s.chm == ChMode::ms ? "S" : "R" };
    const auto h = proc.hostTime();
    const juce::String bpm = juce::String (h.bpm, 2).trimCharactersAtEnd ("0").trimCharactersAtEnd (".");

    juce::String c;
    const juce::String unit = s.mode == Mode::ms ? "MILLISECONDS" : s.mode == Mode::smp ? "SAMPLES" : "OF A WHOLE NOTE";
    if (s.dual)
    {
        c = unit + sepU + tags[0] + " " + formatSigned (a.samples) + sepU + tags[1] + " " + formatSigned (b.samples) + " SMP"
          + sepU + "GAP " + formatSigned (b.samples - a.samples) + (s.sync ? sepU + "SYNC" : juce::String());
    }
    else if (s.mode == Mode::ms)
        c = unit + sepU + formatSigned (a.samples) + " SMP @ " + sampleRateText() + sepU + dir;
    else if (s.mode == Mode::smp)
        c = unit + " @ " + sampleRateText() + sepU + fmtMs (ms) + " MS" + sepU + dir;
    else
        c = unit + sepU + fmtMs (ms) + " MS" + sepU + formatSigned (a.samples) + " SMP @ " + bpm + " BPM" + sepU + dir;

    if (s.mode == Mode::note)
    {
        juce::StringArray base;
        for (int i = 0; i < (s.dual ? 2 : 1); ++i)
            if (std::llround (s.lanes[i].baseMs * rate / 1000.0) != 0)
                base.add ((s.dual ? tags[i] + " " : juce::String()) + formatSigned (std::llround (s.lanes[i].baseMs * rate / 1000.0)) + " SMP");
        if (! base.isEmpty())
            c += sepU + "BASE " + base.joinIntoString (" ");
    }

    if (a.limited || (s.dual && b.limited))
    {
        const int sug = proc.suggestedAdvance();
        if (sug >= 0)
            c += sepU + "SET ADVANCE " + juce::String (kAdvanceMs[sug]) + " MS";
        else if (sug == -2)
            c += sepU + "OVER 2000 MS ADVANCE MAXIMUM";
        else
            c += sepU + "LATER THAN 4 S MAXIMUM";
    }
    return c;
}

void ZamikEditor::refresh()
{
    const auto& s = proc.settings();

    if (s.dual != laidOutDual)
    {
        laidOutDual = s.dual;
        if (! s.dual) balPanel.setVisible (false);
    }

    for (int i = 0; i < kNumSlots; ++i)
    {
        varBtn[i].setToggleState (i == proc.activeSlot(), juce::dontSendNotification);
        const bool used = proc.slotUsed (i) && i != proc.activeSlot();
        if ((bool) varBtn[i].getProperties()["used"] != used) { varBtn[i].getProperties().set ("used", used); varBtn[i].repaint(); }
    }
    for (int i = 0; i < 3; ++i)
        modeBtn[i].setToggleState ((int) s.mode == i, juce::dontSendNotification);

    const bool ms = s.mode == Mode::ms, smp = s.mode == Mode::smp, note = s.mode == Mode::note;
    for (int i = 0; i < kNumSteps; ++i)
    {
        msChips[i].setVisible (ms);
        smpChips[i].setVisible (smp);
        msChips[i].setToggleState (i == s.msStep, juce::dontSendNotification);
        smpChips[i].setToggleState (i == s.smpStep, juce::dontSendNotification);
    }
    for (int i = 0; i < kNumDivs; ++i)
    {
        divChips[i].setVisible (note);
        divChips[i].setToggleState (i == s.div, juce::dontSendNotification);
    }
    for (int i = 0; i < 3; ++i)
    {
        feelChips[i].setVisible (note);
        feelChips[i].setToggleState (i == s.feel, juce::dontSendNotification);
        feelChips[i].setEnabled (s.div != 8);
    }

    if (ms)
    {
        centreLabel = "STEP";
        centreValue = juce::String (kMsStep8[s.msStep] / 8.0, 3) + " MS";
    }
    else if (smp)
    {
        centreLabel = "STEP";
        centreValue = juce::String (kSmpStep[s.smpStep]) + (kSmpStep[s.smpStep] == 1 ? " SAMPLE" : " SAMPLES");
    }
    else
    {
        const auto h = proc.hostTime();
        centreLabel = "DIVISION";
        centreValue = s.div == 8 ? "1 BAR (" + juce::String (h.num) + "/" + juce::String (h.den) + ")"
                                 : juce::String (kDivNames[s.div]) + (s.feel == 1 ? " T" : s.feel == 2 ? " D" : "");
    }

    // arrows
    const juce::String tags[2] = { s.chm == ChMode::ms ? "M" : "L", s.chm == ChMode::ms ? "S" : "R" };
    bigEarly.setVisible (! s.dual);
    bigLate.setVisible (! s.dual);
    for (int i = 0; i < 2; ++i)
    {
        laneEarly[i].setVisible (s.dual);
        laneLate[i].setVisible (s.dual);
        laneEarly[i].setLabel (tags[i]);
        laneLate[i].setLabel (tags[i]);
    }
    laneEarly[1].setEnabled (! s.sync);
    laneLate[1].setEnabled (! s.sync);

    // footer
    snapBtn.setVisible (note);
    snapBtn.setEnabled (proc.canSnap());
    dualBtn.setButtonText (s.dual ? "SINGLE" : "DUAL");
    dualBtn.setTooltip (s.dual ? "Back to one display for both channels" : "Two displays, one per channel");
    chLR.setVisible (s.dual);
    chMS.setVisible (s.dual);
    chLR.setToggleState (s.chm == ChMode::lr, juce::dontSendNotification);
    chMS.setToggleState (s.chm == ChMode::ms, juce::dontSendNotification);
    balBtn.setVisible (s.dual && s.chm == ChMode::ms);
    balBtn.setButtonText (s.bal == 0 ? "BAL" : s.bal < 0 ? "BAL M" + juce::String (-s.bal) : "BAL S" + juce::String (s.bal));
    balBtn.setToggleState (s.bal != 0, juce::dontSendNotification);
    if (! balBtn.isVisible()) balPanel.setVisible (false);
    syncBtn.setVisible (s.dual);
    syncBtn.setToggleState (s.sync, juce::dontSendNotification);
    bypassBtn.setToggleState (proc.isBypassed(), juce::dontSendNotification);
    advanceBox.setSelectedId (s.advIndex + 1, juce::dontSendNotification);
    balPanel.refresh();

    // display
    const auto h = proc.hostTime();
    const double rate = proc.sampleRate();
    std::vector<std::vector<Tube>> rows;
    for (int i = 0; i < (s.dual ? 2 : 1); ++i)
    {
        if (ms)       rows.push_back (msTubes (wantedMs (s, i, h)));
        else if (smp) rows.push_back (smpTubes (wantedSamples (s, i, h, rate)));
        else          rows.push_back (noteTubes (s.lanes[i].ticks));
    }

    const auto a = proc.appliedFor (0), b = proc.appliedFor (1);
    const bool limited = ! proc.isBypassed() && (a.limited || (s.dual && b.limited));
    const bool matrix = proc.matrixActive();
    const int sug = proc.suggestedAdvance();

    juce::String tip;
    if (limited)
    {
        for (int i = 0; i < (s.dual ? 2 : 1); ++i)
        {
            const auto w = wantedSamples (s, i, h, rate), ap = proc.appliedFor (i).samples;
            if (w != ap)
                tip << (s.dual ? tags[i] + ": " : juce::String()) << "requested " << formatSigned (w) << " smp, applied " << formatSigned (ap) << " smp\n";
        }
        if (sug >= 0) tip << "Click to set ADVANCE RANGE to " << kAdvanceMs[sug] << " ms.";
        tip = tip.trimEnd();
    }

    const auto caption = captionText();
    juce::String key;
    for (auto& r : rows) for (auto& t : r) key << juce::String::charToString (t.ch) << (t.lit ? "1" : "0") << (t.dp ? "." : "");
    key << "|" << caption << "|" << (int) matrix << (int) limited << (int) proc.isBypassed() << tip << tags[0];
    if (key != lastDisplayKey)
    {
        lastDisplayKey = key;
        display.setRows (std::move (rows), { tags[0], tags[1] });
        display.setCaption (caption);
        display.setLamps (! matrix, s.dual && s.chm == ChMode::ms, matrix, limited, tip, limited && sug >= 0);
        display.setDimmed (proc.isBypassed());
    }

    // relayout and repaint the faceplate only when something on it changed
    juce::String plateKey;
    plateKey << (int) s.dual << (int) s.mode << (int) s.chm << (int) note << centreLabel << centreValue
             << (int) snapBtn.isVisible() << (int) balBtn.isVisible() << balBtn.getButtonText();
    if (plateKey != lastPlateKey)
    {
        lastPlateKey = plateKey;
        layoutFor (s.dual);
        repaint();
    }
}

void ZamikEditor::timerCallback()
{
    // host tempo, sample rate and automation can change on their own
    refresh();
}

void ZamikEditor::toggleBalance()
{
    balPanel.refresh();
    balPanel.setVisible (! balPanel.isVisible());
    if (balPanel.isVisible()) balPanel.toFront (false);
}

void ZamikEditor::mouseDown (const juce::MouseEvent& e)
{
    if (! balPanel.isVisible()) return;
    auto* c = e.eventComponent;
    if (c == &balPanel || balPanel.isParentOf (c) || c == &balBtn)
        return;
    balPanel.setVisible (false);
}

bool ZamikEditor::keyPressed (const juce::KeyPress& k)
{
    const auto& s = proc.settings();
    const int lane = k.getModifiers().isShiftDown() ? 1 : 0;
    if (k.isKeyCode (juce::KeyPress::leftKey))  { proc.nudge (-1, lane); refresh(); return true; }
    if (k.isKeyCode (juce::KeyPress::rightKey)) { proc.nudge (+1, lane); refresh(); return true; }
    if (k.isKeyCode (juce::KeyPress::escapeKey) && balPanel.isVisible()) { balPanel.setVisible (false); return true; }

    const auto c = juce::CharacterFunctions::toLowerCase (k.getTextCharacter());
    if (c == '0') { proc.zero(); refresh(); return true; }
    if (c == 'm') { proc.setMode ((Mode) (((int) s.mode + 1) % 3)); refresh(); return true; }
    if (c >= 'a' && c <= 'd' && ! k.getModifiers().isCommandDown()) { proc.pickSlot ((int) (c - 'a')); refresh(); return true; }
    return false;
}
