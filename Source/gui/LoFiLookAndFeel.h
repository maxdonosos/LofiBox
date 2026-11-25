#pragma once
//==============================================================================
//  LookAndFeel "cassette": fondo oscuro cálido, crema + naranja.
//  Tipografías embebidas (OFL): Righteous (display) + Space Mono (labels).
//==============================================================================
#include <juce_gui_basics/juce_gui_basics.h>

namespace lofi
{
namespace Palette
{
    const juce::Colour bg         { 0xff15110e };
    const juce::Colour bgLight    { 0xff1d1813 };
    const juce::Colour panel      { 0xff221b16 };
    const juce::Colour panelHi    { 0xff2c231c };
    const juce::Colour edge       { 0xff3d3128 };
    const juce::Colour cream      { 0xffefe2c8 };
    const juce::Colour creamDim   { 0xffa3957d };
    const juce::Colour creamFaint { 0xff5f5446 };
    const juce::Colour orange     { 0xffe8833a };
    const juce::Colour orangeHot  { 0xffffa75e };
    const juce::Colour rust       { 0xffb8532a };
    const juce::Colour brown      { 0xff6e3b22 };
    const juce::Colour red        { 0xffd9472b };
    const juce::Colour ink        { 0xff1a1410 };
}

/** Fuente compatible JUCE 7 / JUCE 8. */
juce::Font makeFont (juce::Typeface::Ptr tf, float height, float kerning = 0.0f);

class LoFiLookAndFeel : public juce::LookAndFeel_V4
{
public:
    LoFiLookAndFeel();

    juce::Font titleFont (float h) const   { return makeFont (display, h, 0.04f); }
    juce::Font labelFont (float h) const   { return makeFont (monoBold, h, 0.06f); }
    juce::Font valueFont (float h) const   { return makeFont (monoRegular, h); }

    // --- overrides
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    juce::Font getPopupMenuFont() override;

    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                           juce::Rectangle<int> parentArea) override;

    juce::Typeface::Ptr display, monoBold, monoRegular;
};
} // namespace lofi
