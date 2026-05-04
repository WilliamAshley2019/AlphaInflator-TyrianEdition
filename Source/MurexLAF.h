//==============================================================================
// MurexLAF.h  —  Tyrian Charcoal Metal Look-and-Feel
//
// Theme: ancient Phoenician murex dye meets industrial hardware.
// Deep charcoal metal faces, Tyrian purple LED accents, amber LCD text,
// black-metal knobs and sliders inspired by the BlackMetal* LNF files.
//==============================================================================

#pragma once
#include <JuceHeader.h>

//==============================================================================
// Colour constants
//==============================================================================
namespace Murex
{
    // ── Metals ────────────────────────────────────────────────────────────────
    constexpr uint32_t FACE = 0xFF1A1A1Fu;   // near-black charcoal face
    constexpr uint32_t FACE_MID = 0xFF222228u;   // panel mid-tone
    constexpr uint32_t FACE_LIGHT = 0xFF2C2C36u;   // raised panel highlight
    constexpr uint32_t KNOB_DARK = 0xFF0A0A0Au;   // knob outer rim
    constexpr uint32_t KNOB_MID = 0xFF2D2D2Du;   // knob body top
    constexpr uint32_t KNOB_SHADE = 0xFF0F0F0Fu;   // knob body bottom
    constexpr uint32_t SCREW = 0xFF3A3A3Au;   // corner screws
    constexpr uint32_t ENGRAVED = 0xFF141418u;   // engraved text / grooves

    // ── Tyrian Purple family ───────────────────────────────────────────────────
    // True Tyrian purple is a deep red-violet (Dibromoindigo dye)
    constexpr uint32_t TYRIAN = 0xFF7A1F5Cu;   // core murex purple
    constexpr uint32_t TYRIAN_BRIGHT = 0xFFAA2E88u;   // lit LED / accent
    constexpr uint32_t TYRIAN_DIM = 0xFF3D0F2Eu;   // unlit LED / shadow
    constexpr uint32_t TYRIAN_GLOW = 0xFF6B1A50u;   // glow halo
    constexpr uint32_t TYRIAN_PALE = 0xFFCC80B0u;   // text on dark

    // ── Amber LCD ─────────────────────────────────────────────────────────────
    constexpr uint32_t LCD_BG = 0xFF1A1500u;   // LCD background
    constexpr uint32_t LCD_ACTIVE = 0xFFD4960Au;   // lit segment
    constexpr uint32_t LCD_INACTIVE = 0xFF2A2400u;   // unlit segment
    constexpr uint32_t LCD_TEXT = 0xFFBB8800u;   // amber text
    constexpr uint32_t LCD_BORDER = 0xFF3A3200u;   // LCD bezel

    // ── LED segment colours ───────────────────────────────────────────────────
    constexpr uint32_t LED_GREEN = 0xFF00C840u;   // normal level
    constexpr uint32_t LED_GREEN_DIM = 0xFF003A10u;
    constexpr uint32_t LED_YELLOW = 0xFFD4B000u;   // approaching clip
    constexpr uint32_t LED_YEL_DIM = 0xFF3A3000u;
    constexpr uint32_t LED_RED = 0xFFD43000u;   // clip
    constexpr uint32_t LED_RED_DIM = 0xFF3A0A00u;
    constexpr uint32_t LED_PURPLE = 0xFFAA2E88u;   // effect/GR LEDs
    constexpr uint32_t LED_PUR_DIM = 0xFF2A0A1Au;

    // ── Text ──────────────────────────────────────────────────────────────────
    constexpr uint32_t TEXT_BRIGHT = 0xFFE0D8E8u;   // main labels
    constexpr uint32_t TEXT_MID = 0xFF907890u;   // secondary
    constexpr uint32_t TEXT_DIM = 0xFF504060u;   // inactive

    // ── Borders / rims ────────────────────────────────────────────────────────
    constexpr uint32_t RIM_OUTER = 0xFF0A0A0Cu;   // outer bevel dark
    constexpr uint32_t RIM_INNER = 0xFF3A3848u;   // inner bevel light
    constexpr uint32_t RIM_ACCENT = 0xFF6B1A50u;   // purple bevel line
}

//==============================================================================
// Amber LCD block display  (drop-in for ThinBlockLcdDisplay style)
//==============================================================================
class MurexLcdDisplay : public juce::Component
{
public:
    MurexLcdDisplay() { setSize(100, 14); }

    void setValue(float normalised)
    {
        value = juce::jlimit(0.0f, 1.0f, normalised);
        repaint();
    }
    void setValueText(const juce::String& t) { text = t; repaint(); }

    void paint(juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();

        // Bezel
        g.setColour(juce::Colour(Murex::LCD_BORDER));
        g.fillRoundedRectangle(b, 2.0f);

        // Background
        const auto inner = b.reduced(1.5f);
        g.setColour(juce::Colour(Murex::LCD_BG));
        g.fillRoundedRectangle(inner, 1.5f);

        // Segments
        constexpr int N = 12;
        const float segW = (inner.getWidth() - 28.0f) / N;
        const int lit = juce::jlimit(0, N, (int)std::round(value * N));

        for (int i = 0; i < N; ++i)
        {
            const float sx = inner.getX() + 1.0f + i * (segW + 1.0f);
            const float sy = inner.getY() + 1.5f;
            const float sh = inner.getHeight() - 3.0f;
            g.setColour(i < lit ? juce::Colour(Murex::LCD_ACTIVE)
                : juce::Colour(Murex::LCD_INACTIVE));
            g.fillRoundedRectangle(sx, sy, segW - 0.5f, sh, 0.8f);
        }

        // Value text
        g.setColour(juce::Colour(Murex::LCD_TEXT));
        g.setFont(juce::Font(juce::FontOptions{}
            .withName(juce::Font::getDefaultMonospacedFontName())
            .withHeight(8.5f)));
        g.drawText(text, (int)(inner.getRight() - 27), (int)inner.getY(),
            26, (int)inner.getHeight(), juce::Justification::centredRight);
    }

private:
    float value = 0.0f;
    juce::String text;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MurexLcdDisplay)
};

//==============================================================================
// Vertical LED bar meter  (segmented, colour-coded)
//==============================================================================
class MurexLedMeter : public juce::Component
{
public:
    enum class Mode { Level, GainReduction, Effect };

    MurexLedMeter() = default;

    void setMode(Mode m) { mode = m; repaint(); }

    void setLevels(float ppm, float rms, float peak)
    {
        ppmVal = juce::jlimit(0.0f, 1.0f, ppm);
        rmsVal = juce::jlimit(0.0f, 1.0f, rms);
        peakVal = juce::jlimit(0.0f, 1.0f, peak);
        repaint();
    }

    void setOvers(bool o) { hasOvers = o; repaint(); }

    void paint(juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat().reduced(1.0f);
        const float H = b.getHeight();
        constexpr int SEGS = 24;
        const float segH = H / SEGS;
        const float segGap = 1.5f;

        // Background groove
        g.setColour(juce::Colour(Murex::ENGRAVED));
        g.fillRoundedRectangle(b, 3.0f);

        // Segment thresholds (bottom = 0, top = 1)
        // Green: 0..0.75   Yellow: 0.75..0.90   Red: 0.90..1.0
        const float greenTop = 0.75f;
        const float yellowTop = 0.90f;

        for (int i = 0; i < SEGS; ++i)
        {
            const float frac = (float)i / SEGS;   // 0=bottom, 1=top
            const float segY = b.getBottom() - (i + 1) * segH + segGap * 0.5f;
            const float sw = b.getWidth() - 4.0f;
            const float sx = b.getX() + 2.0f;
            const float sh = segH - segGap;

            bool lit = false;
            juce::Colour litCol, dimCol;

            if (mode == Mode::GainReduction || mode == Mode::Effect)
            {
                lit = frac < ppmVal;
                litCol = juce::Colour(Murex::LED_PURPLE);
                dimCol = juce::Colour(Murex::LED_PUR_DIM);
            }
            else
            {
                if (frac < greenTop) { litCol = juce::Colour(Murex::LED_GREEN);  dimCol = juce::Colour(Murex::LED_GREEN_DIM); }
                else if (frac < yellowTop) { litCol = juce::Colour(Murex::LED_YELLOW); dimCol = juce::Colour(Murex::LED_YEL_DIM); }
                else {
                    litCol = juce::Colour(hasOvers ? Murex::TYRIAN_BRIGHT : Murex::LED_RED);
                    dimCol = juce::Colour(Murex::LED_RED_DIM);
                }
                lit = frac < ppmVal;
            }

            g.setColour(lit ? litCol : dimCol);
            g.fillRoundedRectangle(sx, segY, sw, sh, 1.0f);
        }

        // RMS overlay tick (white line)
        if (mode == Mode::Level)
        {
            const float rmsY = b.getBottom() - rmsVal * H;
            g.setColour(juce::Colour(0x80FFFFFFu));
            g.fillRect(b.getX() + 1.0f, rmsY - 1.0f, b.getWidth() - 2.0f, 2.0f);
        }

        // Peak hold tick
        if (peakVal > 0.01f)
        {
            const float pkY = b.getBottom() - peakVal * H - 1.5f;
            const bool  clip = peakVal >= 0.99f;
            g.setColour(clip ? juce::Colour(Murex::TYRIAN_BRIGHT)
                : juce::Colour(0xCCFFFFFFu));
            g.fillRect(b.getX() + 1.0f, pkY, b.getWidth() - 2.0f, 2.5f);
        }

        // Border
        g.setColour(juce::Colour(Murex::RIM_OUTER));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 3.0f, 1.0f);
    }

private:
    float ppmVal = 0, rmsVal = 0, peakVal = 0;
    bool  hasOvers = false;
    Mode  mode = Mode::Level;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MurexLedMeter)
};

//==============================================================================
// Knob + LCD parameter widget
//==============================================================================
class MurexKnobWidget : public juce::Component,
    private juce::Slider::Listener
{
public:
    MurexKnobWidget()
    {
        knob.setSliderStyle(juce::Slider::Rotary);
        knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        knob.addListener(this);
        addAndMakeVisible(knob);
        addAndMakeVisible(lcd);
        addAndMakeVisible(nameLabel);

        nameLabel.setFont(juce::Font(juce::FontOptions{}
        .withHeight(9.5f).withStyle("Bold")));
        nameLabel.setColour(juce::Label::textColourId,
            juce::Colour(Murex::TEXT_MID));
        nameLabel.setJustificationType(juce::Justification::centred);
    }

    void setup(const juce::String& name, const juce::String& unitStr)
    {
        nameLabel.setText(name, juce::dontSendNotification);
        unit = unitStr;
    }

    juce::Slider& getSlider() { return knob; }

    void resized() override
    {
        auto b = getLocalBounds();
        nameLabel.setBounds(b.removeFromTop(13));
        lcd.setBounds(b.removeFromBottom(14));
        b.removeFromBottom(2);
        knob.setBounds(b.reduced(2));
    }

    void sliderValueChanged(juce::Slider*) override { updateLcd(); }
    void updateLcd()
    {
        const double v = knob.getValue();
        const double mn = knob.getMinimum(), mx = knob.getMaximum();
        const float norm = static_cast<float> ((v - mn) / (mx - mn));
        lcd.setValue(norm);
        lcd.setValueText(juce::String(v, 1) + unit);
    }

private:
    juce::Slider    knob;
    MurexLcdDisplay lcd;
    juce::Label     nameLabel;
    juce::String    unit;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MurexKnobWidget)
};

//==============================================================================
// Murex Look-and-Feel  — black metal body + Tyrian purple LEDs
//==============================================================================
class MurexLAF : public juce::LookAndFeel_V4
{
public:
    MurexLAF()
    {
        using C = juce::Colour;
        setColour(juce::ComboBox::backgroundColourId, C(Murex::FACE_LIGHT));
        setColour(juce::ComboBox::outlineColourId, C(Murex::RIM_OUTER));
        setColour(juce::ComboBox::textColourId, C(Murex::TEXT_BRIGHT));
        setColour(juce::ComboBox::arrowColourId, C(Murex::TYRIAN_BRIGHT));
        setColour(juce::PopupMenu::backgroundColourId, C(Murex::FACE_MID));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, C(Murex::TYRIAN).withAlpha(0.4f));
        setColour(juce::PopupMenu::textColourId, C(Murex::TEXT_BRIGHT));
        setColour(juce::PopupMenu::highlightedTextColourId, C(Murex::TYRIAN_PALE));
        setColour(juce::Slider::textBoxTextColourId, C(Murex::LCD_TEXT));
        setColour(juce::Slider::textBoxBackgroundColourId, C(0x00000000u));
        setColour(juce::Slider::textBoxOutlineColourId, C(0x00000000u));
        setColour(juce::TextButton::buttonColourId, C(Murex::FACE_LIGHT));
        setColour(juce::TextButton::textColourOffId, C(Murex::TEXT_MID));
        setColour(juce::TextButton::buttonOnColourId, C(Murex::TYRIAN));
        setColour(juce::TextButton::textColourOnId, C(Murex::TYRIAN_PALE));
    }

    //--------------------------------------------------------------------------
    // Rotary knob — black metal style (from BlackMetalKnobLNF, adapted)
    //--------------------------------------------------------------------------
    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
        float sliderPos, float startAngle, float endAngle,
        juce::Slider&) override
    {
        const auto bounds = juce::Rectangle<float>(
            (float)x, (float)y, (float)w, (float)h).reduced(5.0f);
        const float radius = bounds.getWidth() * 0.5f;
        const float cx = bounds.getCentreX(), cy = bounds.getCentreY();
        const float angle = startAngle + sliderPos * (endAngle - startAngle);

        // Shadow disc
        g.setColour(juce::Colour(0x60000000u));
        g.fillEllipse(bounds.translated(1.5f, 2.0f));

        // Outer metal ring
        g.setColour(juce::Colour(Murex::KNOB_DARK));
        g.fillEllipse(bounds);

        // Purple arc track (background)
        {
            const float ar = radius - 2.0f;
            juce::Path bg;
            bg.addArc(cx - ar, cy - ar, ar * 2.0f, ar * 2.0f, startAngle, endAngle, true);
            g.setColour(juce::Colour(Murex::TYRIAN_DIM));
            g.strokePath(bg, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved,
                juce::PathStrokeType::rounded));
        }
        // Purple arc track (active)
        {
            const float ar = radius - 2.0f;
            juce::Path active;
            active.addArc(cx - ar, cy - ar, ar * 2.0f, ar * 2.0f, startAngle, angle, true);
            g.setColour(juce::Colour(Murex::TYRIAN_BRIGHT));
            g.strokePath(active, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved,
                juce::PathStrokeType::rounded));
        }

        // Inner body gradient (black metal)
        {
            const float ir = radius * 0.72f;
            juce::ColourGradient grad(
                juce::Colour(Murex::KNOB_MID), cx, cy - ir,
                juce::Colour(Murex::KNOB_SHADE), cx, cy + ir, false);
            g.setGradientFill(grad);
            g.fillEllipse(cx - ir, cy - ir, ir * 2.0f, ir * 2.0f);
        }

        // Engraved concentric ring
        {
            const float er = radius * 0.58f;
            g.setColour(juce::Colour(Murex::ENGRAVED));
            g.drawEllipse(cx - er, cy - er, er * 2.0f, er * 2.0f, 0.8f);
        }

        // Indicator line (white, like BlackMetalKnobLNF)
        {
            const float lineLen = radius * 0.42f;
            const float lineW = 2.0f;
            juce::Path p;
            p.addRectangle(-lineW * 0.5f, -radius * 0.62f, lineW, lineLen);
            g.setColour(juce::Colours::white);
            g.fillPath(p, juce::AffineTransform::rotation(angle).translated(cx, cy));
        }

        // Centre jewel (tiny purple dot)
        g.setColour(juce::Colour(Murex::TYRIAN_BRIGHT).withAlpha(0.7f));
        g.fillEllipse(cx - 2.0f, cy - 2.0f, 4.0f, 4.0f);
    }

    //--------------------------------------------------------------------------
    // Toggle button — LED lozenge style
    //--------------------------------------------------------------------------
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& btn,
        bool highlighted, bool) override
    {
        const bool on = btn.getToggleState();
        const auto bounds = btn.getLocalBounds().toFloat().reduced(1.0f);
        const float r = 4.0f;

        // Body
        juce::ColourGradient body(
            juce::Colour(on ? Murex::FACE_LIGHT : Murex::ENGRAVED).brighter(highlighted ? 0.08f : 0.0f),
            bounds.getX(), bounds.getY(),
            juce::Colour(Murex::KNOB_DARK),
            bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill(body);
        g.fillRoundedRectangle(bounds, r);

        // LED indicator dot (top-left)
        const float dotR = 3.5f;
        const float dotX = bounds.getX() + 6.0f;
        const float dotY = bounds.getCentreY();
        g.setColour(on ? juce::Colour(Murex::TYRIAN_BRIGHT)
            : juce::Colour(Murex::TYRIAN_DIM));
        g.fillEllipse(dotX - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f);
        // LED glow halo when on
        if (on)
        {
            g.setColour(juce::Colour(Murex::TYRIAN_GLOW).withAlpha(0.5f));
            g.fillEllipse(dotX - dotR * 2.0f, dotY - dotR * 2.0f, dotR * 4.0f, dotR * 4.0f);
        }

        // Rim
        g.setColour(on ? juce::Colour(Murex::TYRIAN)
            : juce::Colour(Murex::RIM_OUTER));
        g.drawRoundedRectangle(bounds.reduced(0.5f), r, 1.0f);

        // Text
        g.setColour(on ? juce::Colour(Murex::TYRIAN_PALE) : juce::Colour(Murex::TEXT_DIM));
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(10.5f).withStyle("Bold")));
        const auto textArea = bounds.withTrimmedLeft(14.0f);
        g.drawFittedText(btn.getButtonText(), textArea.toNearestInt(),
            juce::Justification::centredLeft, 1);
    }

    //--------------------------------------------------------------------------
    // Linear slider — black metal horizontal (from BlackMetalSliderLNF)
    //--------------------------------------------------------------------------
    void drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h,
        float sliderPos, float, float,
        const juce::Slider::SliderStyle style,
        juce::Slider& slider) override
    {
        if (style == juce::Slider::LinearVertical)
        {
            drawVerticalSlider(g, x, y, w, h, sliderPos);
            return;
        }

        const auto bounds = juce::Rectangle<float>(
            (float)x, (float)y, (float)w, (float)h).reduced(2.0f);
        const float trackH = 4.0f;
        const float centerY = bounds.getCentreY();

        // Track BG
        g.setColour(juce::Colour(Murex::KNOB_DARK));
        g.fillRoundedRectangle(bounds.getX(), centerY - trackH * 0.5f,
            bounds.getWidth(), trackH, trackH * 0.5f);

        // Track fill (purple)
        const float fillW = sliderPos - bounds.getX();
        if (fillW > 0.0f)
        {
            juce::ColourGradient grad(
                juce::Colour(Murex::TYRIAN), bounds.getX(), centerY,
                juce::Colour(Murex::TYRIAN_BRIGHT), bounds.getX() + fillW, centerY, false);
            g.setGradientFill(grad);
            g.fillRoundedRectangle(bounds.getX(), centerY - trackH * 0.5f,
                fillW, trackH, trackH * 0.5f);
        }

        // Thumb
        const float tw = 10.0f, th = bounds.getHeight() * 0.7f;
        const float tx = sliderPos - tw * 0.5f, ty = centerY - th * 0.5f;
        juce::ColourGradient tg(juce::Colour(Murex::KNOB_MID), tx + tw * 0.5f, ty,
            juce::Colour(Murex::KNOB_SHADE), tx + tw * 0.5f, ty + th, false);
        g.setGradientFill(tg);
        g.fillRoundedRectangle(tx, ty, tw, th, 2.5f);
        g.setColour(juce::Colour(Murex::TYRIAN));
        g.drawRoundedRectangle(tx, ty, tw, th, 2.5f, 1.0f);
        g.setColour(juce::Colour(Murex::TYRIAN_PALE).withAlpha(0.6f));
        g.drawLine(tx + tw * 0.5f, ty + 2.0f, tx + tw * 0.5f, ty + th - 2.0f, 0.8f);

        juce::ignoreUnused(slider);
    }

    //--------------------------------------------------------------------------
    // ComboBox
    //--------------------------------------------------------------------------
    void drawComboBox(juce::Graphics& g, int w, int h, bool,
        int, int, int, int, juce::ComboBox&) override
    {
        const auto b = juce::Rectangle<float>(0, 0, (float)w, (float)h);
        g.setColour(juce::Colour(Murex::FACE_LIGHT));
        g.fillRoundedRectangle(b, 3.0f);
        g.setColour(juce::Colour(Murex::TYRIAN_DIM));
        g.drawRoundedRectangle(b.reduced(0.5f), 3.0f, 1.0f);

        // Arrow
        const float ax = (float)w - 12.0f, ay = (float)h * 0.5f;
        juce::Path arrow;
        arrow.addTriangle(ax - 4.0f, ay - 2.5f, ax + 4.0f, ay - 2.5f, ax, ay + 3.0f);
        g.setColour(juce::Colour(Murex::TYRIAN_BRIGHT));
        g.fillPath(arrow);
    }

    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
        bool isSep, bool isActive, bool isHighlighted,
        bool isTicked, bool, const juce::String& text,
        const juce::String&, const juce::Drawable*,
        const juce::Colour*) override
    {
        if (isSep)
        {
            g.setColour(juce::Colour(Murex::TYRIAN_DIM));
            g.fillRect(area.getX() + 4, area.getCentreY(), area.getWidth() - 8, 1);
            return;
        }
        if (isHighlighted)
        {
            g.setColour(juce::Colour(Murex::TYRIAN).withAlpha(0.3f));
            g.fillRect(area);
        }
        g.setColour(isTicked ? juce::Colour(Murex::TYRIAN_BRIGHT)
            : (isActive ? juce::Colour(Murex::TEXT_BRIGHT)
                : juce::Colour(Murex::TEXT_DIM)));
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(12.5f)));
        g.drawFittedText(text, area.reduced(8, 0),
            juce::Justification::centredLeft, 1);
    }

private:
    void drawVerticalSlider(juce::Graphics& g, int x, int y, int w, int h,
        float sliderPos)
    {
        const auto bounds = juce::Rectangle<float>(
            (float)x, (float)y, (float)w, (float)h).reduced(2.0f);
        const float trackW = 4.0f;
        const float centerX = bounds.getCentreX();

        g.setColour(juce::Colour(Murex::KNOB_DARK));
        g.fillRoundedRectangle(centerX - trackW * 0.5f, bounds.getY(),
            trackW, bounds.getHeight(), trackW * 0.5f);

        const float fillH = bounds.getBottom() - sliderPos;
        if (fillH > 0.0f)
        {
            juce::ColourGradient grad(
                juce::Colour(Murex::TYRIAN_BRIGHT), centerX, bounds.getBottom(),
                juce::Colour(Murex::TYRIAN), centerX, sliderPos, false);
            g.setGradientFill(grad);
            g.fillRoundedRectangle(centerX - trackW * 0.5f, sliderPos,
                trackW, fillH, trackW * 0.5f);
        }

        const float th = 10.0f, tw = bounds.getWidth() * 0.7f;
        const float tx = centerX - tw * 0.5f, ty = sliderPos - th * 0.5f;
        juce::ColourGradient tg(juce::Colour(Murex::KNOB_MID), tx, ty + th * 0.5f,
            juce::Colour(Murex::KNOB_SHADE), tx + tw, ty + th * 0.5f, false);
        g.setGradientFill(tg);
        g.fillRoundedRectangle(tx, ty, tw, th, 2.5f);
        g.setColour(juce::Colour(Murex::TYRIAN));
        g.drawRoundedRectangle(tx, ty, tw, th, 2.5f, 1.0f);
    }
};