#pragma once
#include "PluginProcessor.h"
#include "MurexLAF.h"

struct ABSnapshot
{
    juce::HashMap<juce::String, float> values;
    bool valid = false;
};

class StereoLedMeter : public juce::Component
{
public:
    explicit StereoLedMeter(const juce::String& lbl, MurexLedMeter::Mode mode = MurexLedMeter::Mode::Level);
    void setLevels(float ppL, float rmL, float pkL, float ppR, float rmR, float pkR);
    void setOvers(bool L, bool R);
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    MurexLedMeter left, right;
    juce::String label;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StereoLedMeter)
};

class HorizLedBar : public juce::Component
{
public:
    HorizLedBar() = default;
    void setValue(float v) { value = juce::jlimit(0.0f, 1.0f, v); repaint(); }
    void paint(juce::Graphics&) override;
private:
    float value = 0.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HorizLedBar)
};

class ScrewDecal : public juce::Component
{
public:
    void paint(juce::Graphics& g) override
    {
        const float r = std::min(getWidth(), getHeight()) * 0.4f;
        const float cx = getWidth() * 0.5f, cy = getHeight() * 0.5f;
        g.setColour(juce::Colour(Murex::KNOB_DARK));
        g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
        g.setColour(juce::Colour(Murex::SCREW));
        g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 1.0f);
        g.setColour(juce::Colour(Murex::ENGRAVED));
        g.drawLine(cx - r * 0.55f, cy, cx + r * 0.55f, cy, 1.0f);
        g.drawLine(cx, cy - r * 0.55f, cx, cy + r * 0.55f, 1.0f);
    }
};

class JSInflatorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit JSInflatorEditor(JSInflatorProcessor&);
    ~JSInflatorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    JSInflatorProcessor& proc;
    MurexLAF laf;
    static constexpr int BASE_W = 580, BASE_H = 390;
    float uiScale = 1.0f;

    juce::ComboBox scaleCombo;
    MurexKnobWidget inputKnob, effectKnob, curveKnob, outputKnob, toneKnob;
    juce::Slider limCeilSlider;
    juce::Label limCeilLbl;
    MurexLcdDisplay limCeilLcd;

    juce::ToggleButton inBtn{ "IN" }, splitBtn{ "SPLIT" }, msBtn{ "M/S" }, dcBtn{ "DC BLK" }, limBtn{ "LIMIT" };
    juce::ToggleButton deltaBtn{ "DELTA" }, bypassBtn{ "BYPASS" };

    juce::ComboBox osCombo, osQualCombo, clipCombo, splitTypeCombo, agcCombo, focusCombo, dynCombo;
    juce::Label osLbl, osQualLbl, clipLbl, splitTypeLbl, agcLbl, focusLbl, dynLbl;

    StereoLedMeter inMeter{ "IN" }, outMeter{ "OUT" };
    HorizLedBar effectBar, grBar;
    juce::Label effectBarLbl, grBarLbl;

    juce::TextButton abSaveA{ "SAVE A" }, abSaveB{ "SAVE B" }, abLoadA{ "A" }, abLoadB{ "B" };
    ABSnapshot snapA, snapB;
    void saveSnap(ABSnapshot& s);
    void loadSnap(const ABSnapshot& s);
    void onAB(bool a);

    ScrewDecal screw[4];

    using SlAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BtnAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CbAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SlAtt> attInput, attEffect, attCurve, attOutput, attTone, attLimCeil;
    std::unique_ptr<BtnAtt> attIn, attSplit, attMS, attBypass, attDelta, attLimiter, attDCBlock;
    std::unique_ptr<CbAtt> attOS, attOSQual, attClip, attSplitType, attAGC, attFocus, attDyn;

    void setupKnob(MurexKnobWidget& w, const juce::String& n, const juce::String& u);
    void setupToggle(juce::ToggleButton& b);
    void setupCombo(juce::ComboBox& c, juce::Label& l, const juce::String& t, const juce::StringArray& i);
    void setupTextBtn(juce::TextButton& b);
    void applyScale(float s);
    void timerCallback() override;
    void paintBackground(juce::Graphics&);
    void paintFaceplate(juce::Graphics&);
    void paintSections(juce::Graphics&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JSInflatorEditor)
};