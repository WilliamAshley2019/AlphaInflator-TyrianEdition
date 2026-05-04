//==============================================================================
// JS Inflator  v1.4 - PluginEditor.cpp
//==============================================================================
#include "PluginEditor.h"
using juce::Colour;
using juce::Rectangle;
static inline Colour mc(uint32_t c) { return Colour(c); }

StereoLedMeter::StereoLedMeter(const juce::String& lbl, MurexLedMeter::Mode mode)
    : label(lbl)
{
    left.setMode(mode); right.setMode(mode);
    addAndMakeVisible(left); addAndMakeVisible(right);
}
void StereoLedMeter::setLevels(float pL, float rL, float pkL, float pR, float rR, float pkR)
{
    left.setLevels(pL, rL, pkL); right.setLevels(pR, rR, pkR);
}
void StereoLedMeter::setOvers(bool L, bool R) { left.setOvers(L); right.setOvers(R); }
void StereoLedMeter::paint(juce::Graphics& g)
{
    g.setColour(mc(Murex::TEXT_MID));
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(9.0f).withStyle("Bold")));
    g.drawText(label, getLocalBounds().removeFromBottom(12), juce::Justification::centred);
}
void StereoLedMeter::resized()
{
    auto b = getLocalBounds().withTrimmedBottom(13);
    const int gap = 2, hw = (b.getWidth() - gap) / 2;
    left.setBounds(b.removeFromLeft(hw)); b.removeFromLeft(gap); right.setBounds(b);
}

void HorizLedBar::paint(juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced(1.0f);
    constexpr int N = 20;
    const float segW = b.getWidth() / N;
    const int lit = (int)(value * N);
    for (int i = 0; i < N; ++i)
    {
        const float sx = b.getX() + i * segW + 0.5f;
        const bool on = i < lit;
        g.setColour(on ? mc(Murex::LED_PURPLE) : mc(Murex::LED_PUR_DIM));
        g.fillRoundedRectangle(sx, b.getY() + 0.5f, segW - 1.0f, b.getHeight() - 1.0f, 1.0f);
    }
    g.setColour(mc(Murex::RIM_OUTER));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 2.0f, 1.0f);
}

JSInflatorEditor::JSInflatorEditor(JSInflatorProcessor& p)
    : AudioProcessorEditor(p), proc(p)
{
    setLookAndFeel(&laf);
    setResizable(true, false);
    setResizeLimits(BASE_W, BASE_H, BASE_W * 2, BASE_H * 2);
    setSize(BASE_W, BASE_H);

    scaleCombo.addItem("100%", 1); scaleCombo.addItem("125%", 2);
    scaleCombo.addItem("150%", 3); scaleCombo.addItem("200%", 4);
    scaleCombo.setSelectedId(1, juce::dontSendNotification);
    scaleCombo.onChange = [this]
        {
            const float s[]{ 1.0f, 1.25f, 1.5f, 2.0f };
            applyScale(s[juce::jlimit(0, 3, scaleCombo.getSelectedId() - 1)]);
        };
    addAndMakeVisible(scaleCombo);

    setupKnob(inputKnob, "INPUT", " dB"); setupKnob(effectKnob, "EFFECT", "%");
    setupKnob(curveKnob, "CURVE", ""); setupKnob(outputKnob, "OUTPUT", " dB");
    setupKnob(toneKnob, "TONE", "");

    limCeilSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    limCeilSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(limCeilSlider);
    limCeilLbl.setText("CEIL", juce::dontSendNotification);
    limCeilLbl.setFont(juce::Font(juce::FontOptions{}.withHeight(9.0f).withStyle("Bold")));
    limCeilLbl.setColour(juce::Label::textColourId, mc(Murex::TEXT_MID));
    addAndMakeVisible(limCeilLbl); addAndMakeVisible(limCeilLcd);
    limCeilSlider.onValueChange = [this]
        {
            float v = limCeilSlider.getValue();
            float n = (-v - 0.1f) / 5.9f;
            limCeilLcd.setValue(1 - n);
            limCeilLcd.setValueText(juce::String(v, 2) + " dB");
        };

    for (auto* b : { &inBtn, &splitBtn, &msBtn, &dcBtn, &limBtn, &deltaBtn, &bypassBtn })
        setupToggle(*b);

    setupCombo(osCombo, osLbl, "OVERSAMP", { "1x", "2x", "4x", "8x" });
    setupCombo(osQualCombo, osQualLbl, "OS PHASE", { "Min Phase", "Linear" });
    setupCombo(clipCombo, clipLbl, "CLIP MODE", { "Off", "Hard", "Soft", "H+S" });
    setupCombo(splitTypeCombo, splitTypeLbl, "SPLIT TYPE", { "Simple", "Orig SVF" });
    setupCombo(agcCombo, agcLbl, "AUTO GAIN", { "Off", "Static", "Dynamic" });
    setupCombo(focusCombo, focusLbl, "FOCUS", { "Full", "Low", "Mid", "High" });
    setupCombo(dynCombo, dynLbl, "DYNAMICS", { "Smooth", "Neutral", "Punch", "Dense" });

    for (auto* b : { &abSaveA, &abSaveB, &abLoadA, &abLoadB }) setupTextBtn(*b);
    abSaveA.onClick = [this] { saveSnap(snapA); };
    abSaveB.onClick = [this] { saveSnap(snapB); };
    abLoadA.onClick = [this] { onAB(true); };
    abLoadB.onClick = [this] { onAB(false); };

    addAndMakeVisible(inMeter); addAndMakeVisible(outMeter);
    addAndMakeVisible(effectBar); addAndMakeVisible(grBar);
    auto sl = [&](juce::Label& l, const juce::String& t)
        {
            l.setText(t, juce::dontSendNotification);
            l.setFont(juce::Font(juce::FontOptions{}.withHeight(8.5f).withStyle("Bold")));
            l.setColour(juce::Label::textColourId, mc(Murex::TEXT_MID));
            l.setJustificationType(juce::Justification::centred);
            addAndMakeVisible(l);
        };
    sl(effectBarLbl, "EFFECT"); sl(grBarLbl, "LIM GR");
    for (auto& s : screw) addAndMakeVisible(s);

    auto& a = proc.apvts;
    attInput = std::make_unique<SlAtt>(a, ParamID::INPUT, inputKnob.getSlider());
    attEffect = std::make_unique<SlAtt>(a, ParamID::EFFECT, effectKnob.getSlider());
    attCurve = std::make_unique<SlAtt>(a, ParamID::CURVE, curveKnob.getSlider());
    attOutput = std::make_unique<SlAtt>(a, ParamID::OUTPUT, outputKnob.getSlider());
    attTone = std::make_unique<SlAtt>(a, ParamID::TONE, toneKnob.getSlider());
    attLimCeil = std::make_unique<SlAtt>(a, ParamID::LIM_CEIL, limCeilSlider);
    attIn = std::make_unique<BtnAtt>(a, ParamID::IN, inBtn);
    attSplit = std::make_unique<BtnAtt>(a, ParamID::SPLIT, splitBtn);
    attMS = std::make_unique<BtnAtt>(a, ParamID::MS_MODE, msBtn);
    attDCBlock = std::make_unique<BtnAtt>(a, ParamID::DC_BLOCK, dcBtn);
    attLimiter = std::make_unique<BtnAtt>(a, ParamID::LIMITER, limBtn);
    attDelta = std::make_unique<BtnAtt>(a, ParamID::DELTA, deltaBtn);
    attBypass = std::make_unique<BtnAtt>(a, ParamID::BYPASS, bypassBtn);
    attOS = std::make_unique<CbAtt>(a, ParamID::OS, osCombo);
    attOSQual = std::make_unique<CbAtt>(a, ParamID::OS_QUAL, osQualCombo);
    attClip = std::make_unique<CbAtt>(a, ParamID::CLIP_MODE, clipCombo);
    attSplitType = std::make_unique<CbAtt>(a, ParamID::SPLIT_TYPE, splitTypeCombo);
    attAGC = std::make_unique<CbAtt>(a, ParamID::AGC_MODE, agcCombo);
    attFocus = std::make_unique<CbAtt>(a, ParamID::FOCUS, focusCombo);
    attDyn = std::make_unique<CbAtt>(a, ParamID::DYN_MODE, dynCombo);

    for (auto* w : { &inputKnob, &effectKnob, &curveKnob, &outputKnob, &toneKnob }) w->updateLcd();
    limCeilSlider.setValue(-0.3);
    startTimerHz(30);
}

JSInflatorEditor::~JSInflatorEditor() { stopTimer(); setLookAndFeel(nullptr); }

void JSInflatorEditor::setupKnob(MurexKnobWidget& w, const juce::String& n, const juce::String& u)
{
    w.setup(n, u); addAndMakeVisible(w);
}
void JSInflatorEditor::setupToggle(juce::ToggleButton& b) { b.setClickingTogglesState(true); addAndMakeVisible(b); }
void JSInflatorEditor::setupCombo(juce::ComboBox& c, juce::Label& l, const juce::String& t, const juce::StringArray& i)
{
    c.addItemList(i, 1); c.setScrollWheelEnabled(true); addAndMakeVisible(c);
    l.setText(t, juce::dontSendNotification);
    l.setFont(juce::Font(juce::FontOptions{}.withHeight(8.5f).withStyle("Bold")));
    l.setColour(juce::Label::textColourId, mc(Murex::TEXT_MID));
    l.setJustificationType(juce::Justification::centred); addAndMakeVisible(l);
}
void JSInflatorEditor::setupTextBtn(juce::TextButton& b) { addAndMakeVisible(b); }
void JSInflatorEditor::applyScale(float s) { uiScale = s; setSize(BASE_W * s, BASE_H * s); }

void JSInflatorEditor::saveSnap(ABSnapshot& s)
{
    s.values.clear();
    for (auto* p : proc.apvts.processor.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(p))
            s.values.set(r->getParameterID(), r->getValue());
    s.valid = true;
}
void JSInflatorEditor::loadSnap(const ABSnapshot& s)
{
    if (!s.valid) return;
    for (auto* p : proc.apvts.processor.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(p))
            if (s.values.contains(r->getParameterID()))
                r->setValueNotifyingHost(s.values[r->getParameterID()]);
}
void JSInflatorEditor::onAB(bool loadA)
{
    ABSnapshot& snap = loadA ? snapA : snapB;
    if (!snap.valid) { saveSnap(snap); return; }
    loadSnap(snap);
    abLoadA.setColour(juce::TextButton::buttonColourId, loadA ? mc(Murex::TYRIAN) : mc(Murex::FACE_LIGHT));
    abLoadB.setColour(juce::TextButton::buttonColourId, !loadA ? mc(Murex::TYRIAN) : mc(Murex::FACE_LIGHT));
    repaint();
}

void JSInflatorEditor::timerCallback()
{
    const float pIL = proc.getInLevelL(), pIR = proc.getInLevelR(), pOL = proc.getOutLevelL(), pOR = proc.getOutLevelR();
    const float rIL = proc.getInRmsL(), rIR = proc.getInRmsR(), rOL = proc.getOutRmsL(), rOR = proc.getOutRmsR();
    const float pkIL = proc.getInPeakL(), pkIR = proc.getInPeakR(), pkOL = proc.getOutPeakL(), pkOR = proc.getOutPeakR();
    const int oIL = proc.getAndClearInOversL(), oIR = proc.getAndClearInOversR(), oOL = proc.getAndClearOutOversL(), oOR = proc.getAndClearOutOversR();
    auto toB = [](float l) { if (l <= 0.0f) return 0.0f; return juce::jlimit(0.0f, 1.0f, (juce::Decibels::gainToDecibels(l) + 60.0f) / 66.0f); };
    inMeter.setLevels(toB(pIL), toB(rIL), toB(pkIL), toB(pIR), toB(rIR), toB(pkIR));
    outMeter.setLevels(toB(pOL), toB(rOL), toB(pkOL), toB(pOR), toB(rOR), toB(pkOR));
    inMeter.setOvers(oIL > 0, oIR > 0); outMeter.setOvers(oOL > 0, oOR > 0);
    effectBar.setValue(proc.getEffectMeter());
    grBar.setValue(juce::jlimit(0.0f, 1.0f, -proc.getLimiterGR() / 20.0f));
    for (auto* w : { &inputKnob, &effectKnob, &curveKnob, &outputKnob, &toneKnob }) w->updateLcd();
}

void JSInflatorEditor::paint(juce::Graphics& g) { paintBackground(g); paintFaceplate(g); paintSections(g); }

void JSInflatorEditor::paintBackground(juce::Graphics& g)
{
    juce::ColourGradient bg(mc(0xFF1E1E24u), juce::Point<float>(0.0f, 0.0f),
        mc(0xFF0E0E12u), juce::Point<float>(0.0f, (float)getHeight()), false);
    g.setGradientFill(bg); g.fillAll();
}

void JSInflatorEditor::paintFaceplate(juce::Graphics& g)
{
    const float W = (float)getWidth(), H = (float)getHeight(), s = uiScale;
    g.setColour(mc(Murex::RIM_INNER));
    g.drawLine(0, 0, W, 0, 1.5f); g.drawLine(0, 0, 0, H, 1.5f);
    g.setColour(mc(Murex::RIM_OUTER));
    g.drawLine(0, H - 1, W, H - 1, 1.5f); g.drawLine(W - 1, 0, W - 1, H, 1.5f);

    juce::ColourGradient tb(mc(Murex::TYRIAN_BRIGHT).withAlpha(0.9f), juce::Point<float>(0.0f, 0.0f),
        mc(Murex::TYRIAN_DIM).withAlpha(0.6f), juce::Point<float>(W, 0.0f), false);
    g.setGradientFill(tb); g.fillRect(juce::Rectangle<float>(0.0f, 0.0f, W, 4.0f * s));

    juce::ColourGradient bb(mc(Murex::TYRIAN_DIM), juce::Point<float>(0.0f, H - 4.0f * s),
        mc(Murex::TYRIAN_BRIGHT).withAlpha(0.5f), juce::Point<float>(W, H - 4.0f * s), false);
    g.setGradientFill(bb); g.fillRect(juce::Rectangle<float>(0.0f, H - 4.0f * s, W, 4.0f * s));

    g.setColour(mc(Murex::TEXT_BRIGHT));
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(16.0f * s).withStyle("Bold")));
    g.drawText("Alpha Inflator - Tyrian Edition", juce::Rectangle<float>(12.0f * s, 6.0f * s, 200.0f * s, 22.0f * s), juce::Justification::centredLeft);

    g.setColour(mc(Murex::TYRIAN_PALE));
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(8.5f * s)));
    g.drawText("Oxford-Style Harmonic Exciter | v1.4",
        juce::Rectangle<float>(12.0f * s, 24.0f * s, 380.0f * s, 12.0f * s), juce::Justification::centredLeft);

    g.setColour(mc(Murex::TYRIAN_DIM));
    g.fillRect(juce::Rectangle<float>(0.0f, 40.0f * s, W, 1.0f));
    g.setColour(mc(Murex::RIM_INNER).withAlpha(0.4f));
    g.fillRect(juce::Rectangle<float>(0.0f, 41.0f * s, W, 1.0f));
}

void JSInflatorEditor::paintSections(juce::Graphics& g)
{
    const float s = uiScale;
    const float W = (float)getWidth(), H = (float)getHeight();

    auto drawSectionLabel = [&](const juce::String& text, float x, float y)
        {
            g.setColour(mc(Murex::TYRIAN_PALE).withAlpha(0.5f));
            g.setFont(juce::Font(juce::FontOptions{}.withHeight(8.0f * s).withStyle("Bold")));
            g.drawText(text, juce::Rectangle<float>(x * s, y * s, 80.0f * s, 11.0f * s), juce::Justification::centredLeft);
        };

    drawSectionLabel("CORE", 10, 43); drawSectionLabel("TONE", 360, 43);
    drawSectionLabel("SIGNAL", 10, 185); drawSectionLabel("ENGINE", 10, 280);
    drawSectionLabel("A/B", 440, 185);

    g.setColour(mc(Murex::ENGRAVED));
    g.fillRect(juce::Rectangle<float>(8 * s, 180 * s, W - 16 * s, 1 * s));
    g.setColour(mc(Murex::RIM_INNER).withAlpha(0.25f));
    g.fillRect(juce::Rectangle<float>(8 * s, 181 * s, W - 16 * s, 1 * s));

    g.setColour(mc(Murex::ENGRAVED));
    g.fillRect(juce::Rectangle<float>(8 * s, 276 * s, W - 16 * s, 1 * s));
    g.setColour(mc(Murex::RIM_INNER).withAlpha(0.25f));
    g.fillRect(juce::Rectangle<float>(8 * s, 277 * s, W - 16 * s, 1 * s));

    g.setColour(mc(Murex::ENGRAVED));
    g.fillRect(juce::Rectangle<float>(350 * s, 43 * s, 1 * s, 132 * s));
    g.fillRect(juce::Rectangle<float>(432 * s, 182 * s, 1 * s, 88 * s));
}

void JSInflatorEditor::resized()
{
    uiScale = getWidth() / (float)BASE_W;
    const float s = uiScale;
    auto sc = [&](float v) { return (int)(v * s); };
    auto sr = [&](float x, float y, float w, float h) -> Rectangle<int> { return{ sc(x), sc(y), sc(w), sc(h) }; };

    scaleCombo.setBounds(sr(BASE_W - 62, 8, 58, 20));
    screw[0].setBounds(sr(4, 4, 12, 12)); screw[1].setBounds(sr(BASE_W - 16, 4, 12, 12));
    screw[2].setBounds(sr(4, BASE_H - 16, 12, 12)); screw[3].setBounds(sr(BASE_W - 16, BASE_H - 16, 12, 12));

    const float kT = 54, kW = 62, kH = 80;
    MurexKnobWidget* ks[]{ &inputKnob, &effectKnob, &curveKnob, &outputKnob };
    for (int i = 0; i < 4; ++i) ks[i]->setBounds(sr(10 + i * (kW + 4), kT, kW, kH));
    toneKnob.setBounds(sr(358, kT, kW, kH));

    const float mT = 44, mH = 132, mW = 46;
    inMeter.setBounds(sr(430, mT, mW, mH)); outMeter.setBounds(sr(480, mT, mW, mH));
    effectBarLbl.setBounds(sr(430, mT + mH + 2, 92, 10)); effectBar.setBounds(sr(430, mT + mH + 13, 92, 10));
    grBarLbl.setBounds(sr(430, mT + mH + 25, 92, 10)); grBar.setBounds(sr(430, mT + mH + 36, 92, 10));

    const float bT = 190, bW = 74, bH = 24, bG = 4;
    juce::ToggleButton* r1[]{ &inBtn, &splitBtn, &msBtn, &dcBtn, &limBtn };
    for (int i = 0; i < 5; ++i) r1[i]->setBounds(sr(10 + i * (bW + bG), bT, bW, bH));
    deltaBtn.setBounds(sr(10, bT + bH + bG, 100, bH)); bypassBtn.setBounds(sr(114, bT + bH + bG, 100, bH));
    limCeilLbl.setBounds(sr(225, bT + bH + bG, 50, 11)); limCeilSlider.setBounds(sr(225, bT + bH + bG + 12, 110, 16));
    limCeilLcd.setBounds(sr(338, bT + bH + bG + 8, 64, 14));

    const float aT = 190, aW = 46, aH = 24;
    abSaveA.setBounds(sr(436, aT, aW, aH)); abSaveB.setBounds(sr(436, aT + aH + 4, aW, aH));
    abLoadA.setBounds(sr(486, aT, aW, aH)); abLoadB.setBounds(sr(486, aT + aH + 4, aW, aH));

    const float c1 = 286, c2 = 324, cW = 82, cH = 20, cLH = 11, cG = 6;
    struct CL { juce::ComboBox* c; juce::Label* l; float x, y; };
    for (auto& cl : {
        CL{ &osCombo, &osLbl, 10, c1 }, CL{ &osQualCombo, &osQualLbl, 10 + cW + cG, c1 },
        CL{ &clipCombo, &clipLbl, 10 + 2 * (cW + cG), c1 }, CL{ &splitTypeCombo, &splitTypeLbl, 10 + 3 * (cW + cG), c1 },
        CL{ &agcCombo, &agcLbl, 10, c2 }, CL{ &focusCombo, &focusLbl, 10 + cW + cG, c2 },
        CL{ &dynCombo, &dynLbl, 10 + 2 * (cW + cG), c2 } })
    {
        cl.l->setBounds(sr(cl.x, cl.y - cLH, cW, cLH)); cl.c->setBounds(sr(cl.x, cl.y, cW, cH));
    }
}