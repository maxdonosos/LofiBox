#include "LoFiLookAndFeel.h"
#include "LoFiBoxAssets.h"

namespace lofi
{
juce::Font makeFont (juce::Typeface::Ptr tf, float height, float kerning)
{
   #if JUCE_MAJOR_VERSION >= 8
    juce::Font f (juce::FontOptions (tf).withHeight (height));
   #else
    juce::Font f (tf);
    f.setHeight (height);
   #endif
    if (std::abs (kerning) > 0.0f) f.setExtraKerningFactor (kerning);
    return f;
}

LoFiLookAndFeel::LoFiLookAndFeel()
{
    display     = juce::Typeface::createSystemTypefaceFor (LoFiBoxAssets::RighteousRegular_ttf,  (size_t) LoFiBoxAssets::RighteousRegular_ttfSize);
    monoBold    = juce::Typeface::createSystemTypefaceFor (LoFiBoxAssets::SpaceMonoBold_ttf,     (size_t) LoFiBoxAssets::SpaceMonoBold_ttfSize);
    monoRegular = juce::Typeface::createSystemTypefaceFor (LoFiBoxAssets::SpaceMonoRegular_ttf,  (size_t) LoFiBoxAssets::SpaceMonoRegular_ttfSize);

    using namespace Palette;
    setColour (juce::ResizableWindow::backgroundColourId, bg);
    setColour (juce::PopupMenu::backgroundColourId, panelHi);
    setColour (juce::PopupMenu::textColourId, cream);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, orange);
    setColour (juce::PopupMenu::highlightedTextColourId, ink);
    setColour (juce::PopupMenu::headerTextColourId, orange);
    setColour (juce::ComboBox::textColourId, cream);
    setColour (juce::ComboBox::backgroundColourId, ink);
    setColour (juce::ComboBox::outlineColourId, edge);
    setColour (juce::ComboBox::arrowColourId, orange);
    setColour (juce::TextButton::textColourOffId, cream);
    setColour (juce::TextButton::textColourOnId, ink);
    setColour (juce::TextButton::buttonColourId, ink);
    setColour (juce::TextButton::buttonOnColourId, orange);
    setColour (juce::Label::textColourId, cream);
    setColour (juce::TooltipWindow::textColourId, cream);
    setColour (juce::TooltipWindow::backgroundColourId, ink);
    setColour (juce::TooltipWindow::outlineColourId, orange);
    setColour (juce::AlertWindow::backgroundColourId, panel);
    setColour (juce::AlertWindow::textColourId, cream);
    setColour (juce::AlertWindow::outlineColourId, edge);
    setColour (juce::TextEditor::backgroundColourId, ink);
    setColour (juce::TextEditor::textColourId, cream);
    setColour (juce::TextEditor::outlineColourId, edge);
    setColour (juce::TextEditor::focusedOutlineColourId, orange);
    setColour (juce::TextEditor::highlightColourId, orange.withAlpha (0.4f));
    setColour (juce::CaretComponent::caretColourId, orange);
}

juce::Typeface::Ptr LoFiLookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    // fuente por defecto (popups, alerts, file choosers internos) -> Space Mono
    if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName()
        || f.getTypefaceName() == "<Sans-Serif>")
        return f.isBold() ? monoBold : monoRegular;
    return LookAndFeel_V4::getTypefaceForFont (f);
}

//==============================================================================
//  KNOB
//==============================================================================
void LoFiLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider& s)
{
    using namespace Palette;
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float size  = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto  c     = bounds.getCentre();
    const float r     = size * 0.5f - 1.0f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool  hover = s.isMouseOverOrDragging() && s.isEnabled();
    const float alpha = s.isEnabled() ? 1.0f : 0.45f;

    // --- escala: ticks alrededor
    const int numTicks = 21;
    for (int i = 0; i < numTicks; ++i)
    {
        const float t  = (float) i / (float) (numTicks - 1);
        const float a  = startAngle + t * (endAngle - startAngle);
        const bool major = (i % 5) == 0;
        const float r1 = r * (major ? 0.86f : 0.90f), r2 = r * 0.99f;
        const bool lit = t <= pos + 0.001f;
        g.setColour ((lit ? orange : creamFaint).withMultipliedAlpha (alpha * (major ? 1.0f : 0.75f)));
        juce::Line<float> ln (c.getPointOnCircumference (r1, a), c.getPointOnCircumference (r2, a));
        g.drawLine (ln, major ? 1.6f : 1.0f);
    }

    // --- cuerpo: sombra
    const float bodyR = r * 0.74f;
    juce::Path body; body.addEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2, bodyR * 2);
    juce::DropShadow (juce::Colours::black.withAlpha (0.75f), (int) (size * 0.14f), { 0, (int) (size * 0.05f) }).drawForPath (g, body);

    // anillo exterior moleteado
    {
        juce::ColourGradient grad (juce::Colour (0xff4a3d32), c.x - bodyR, c.y - bodyR,
                                   juce::Colour (0xff120e0b), c.x + bodyR, c.y + bodyR, false);
        g.setGradientFill (grad);
        g.fillPath (body);

        g.setColour (juce::Colours::black.withAlpha (0.55f));
        const int knurls = 40;
        for (int i = 0; i < knurls; ++i)
        {
            const float a = angle + juce::MathConstants<float>::twoPi * (float) i / (float) knurls;
            g.drawLine ({ c.getPointOnCircumference (bodyR * 0.86f, a), c.getPointOnCircumference (bodyR * 0.99f, a) }, 1.0f);
        }
        g.setColour (juce::Colour (0x22ffffff));
        g.drawEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2, bodyR * 2, 1.0f);
    }

    // tapa
    const float capR = bodyR * 0.80f;
    {
        juce::ColourGradient grad (juce::Colour (0xff3a2f27), c.x - capR * 0.6f, c.y - capR * 0.9f,
                                   juce::Colour (0xff17120e), c.x + capR * 0.5f, c.y + capR, true);
        g.setGradientFill (grad);
        g.fillEllipse (c.x - capR, c.y - capR, capR * 2, capR * 2);
        // brillo especular sutil
        juce::ColourGradient spec (juce::Colour (0x26ffffff), c.x, c.y - capR,
                                   juce::Colour (0x00ffffff), c.x, c.y, false);
        g.setGradientFill (spec);
        g.fillEllipse (c.x - capR * 0.85f, c.y - capR * 0.95f, capR * 1.7f, capR * 1.2f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (c.x - capR, c.y - capR, capR * 2, capR * 2, 1.0f);
    }

    // --- puntero (línea crema + punto naranja)
    {
        const auto p1 = c.getPointOnCircumference (capR * 0.22f, angle);
        const auto p2 = c.getPointOnCircumference (capR * 0.86f, angle);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawLine ({ p1.translated (0, 1.0f), p2.translated (0, 1.0f) }, juce::jmax (2.0f, size * 0.04f));
        g.setColour (cream.withMultipliedAlpha (alpha));
        g.drawLine ({ p1, p2 }, juce::jmax (2.0f, size * 0.035f));

        const auto dot = c.getPointOnCircumference (bodyR * 0.93f, angle);
        const float dr = juce::jmax (2.0f, size * 0.035f);
        if (hover)
        {
            g.setColour (orangeHot.withAlpha (0.35f));
            g.fillEllipse (dot.x - dr * 2.2f, dot.y - dr * 2.2f, dr * 4.4f, dr * 4.4f);
        }
        g.setColour ((hover ? orangeHot : orange).withMultipliedAlpha (alpha));
        g.fillEllipse (dot.x - dr, dot.y - dr, dr * 2, dr * 2);
    }
}

//==============================================================================
//  TOGGLE = LED de encendido del módulo
//==============================================================================
void LoFiLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    using namespace Palette;
    const auto area = b.getLocalBounds().toFloat();
    const bool on = b.getToggleState();
    const float d = juce::jmin (area.getHeight(), 14.0f);
    const auto led = juce::Rectangle<float> (d, d).withCentre ({ area.getX() + d * 0.5f + 2.0f, area.getCentreY() });

    if (on)
    {
        juce::ColourGradient halo (orange.withAlpha (0.45f), led.getCentreX(), led.getCentreY(),
                                   orange.withAlpha (0.0f), led.getCentreX() + d * 0.8f, led.getCentreY(), true);
        g.setGradientFill (halo);
        g.fillEllipse (led.expanded (d * 0.3f));
        juce::ColourGradient grad (orangeHot, led.getCentreX() - d * 0.2f, led.getCentreY() - d * 0.25f,
                                   rust, led.getCentreX() + d * 0.4f, led.getBottom(), true);
        g.setGradientFill (grad);
    }
    else
        g.setColour (juce::Colour (0xff2a211b));
    g.fillEllipse (led);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawEllipse (led, 1.0f);
    g.setColour (juce::Colour (0x40ffffff));
    g.fillEllipse (led.reduced (d * 0.3f).translated (-d * 0.12f, -d * 0.15f).withSizeKeepingCentre (d * 0.25f, d * 0.18f));

    g.setColour ((on ? cream : creamDim).withAlpha (highlighted ? 1.0f : 0.85f));
    g.setFont (labelFont (10.5f));
    g.drawText (on ? "ON" : "OFF", area.withTrimmedLeft (d + 7.0f), juce::Justification::centredLeft);
}

//==============================================================================
//  BOTONES DE TEXTO (barra de presets)
//==============================================================================
void LoFiLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                            bool highlighted, bool down)
{
    using namespace Palette;
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const float cr = r.getHeight() * 0.5f;
    g.setColour (down ? orange : (highlighted ? panelHi : ink));
    g.fillRoundedRectangle (r, cr);
    g.setColour (highlighted ? orange.withAlpha (0.8f) : edge);
    g.drawRoundedRectangle (r, cr, 1.0f);
}

void LoFiLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (down ? Palette::ink : Palette::cream);
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

juce::Font LoFiLookAndFeel::getTextButtonFont (juce::TextButton&, int h) { return labelFont (juce::jmin (12.0f, h * 0.48f)); }

//==============================================================================
//  COMBO (selector de presets)
//==============================================================================
void LoFiLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool down,
                                    int, int, int, int, juce::ComboBox& box)
{
    using namespace Palette;
    auto r = juce::Rectangle<float> (0, 0, (float) width, (float) height).reduced (0.5f);
    g.setColour (ink);
    g.fillRoundedRectangle (r, 6.0f);
    // "ventana" tipo display
    juce::ColourGradient sheen (juce::Colour (0x10ffffff), 0, 0, juce::Colour (0x00ffffff), 0, (float) height * 0.6f, false);
    g.setGradientFill (sheen);
    g.fillRoundedRectangle (r.reduced (2.0f), 5.0f);
    g.setColour (box.isMouseOver (true) || down ? orange.withAlpha (0.9f) : edge);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);

    // triangulito
    juce::Path tri;
    const float cx = (float) width - 16.0f, cy = (float) height * 0.5f;
    tri.addTriangle (cx - 4.5f, cy - 2.5f, cx + 4.5f, cy - 2.5f, cx, cy + 3.5f);
    g.setColour (orange);
    g.fillPath (tri);
}

juce::Font LoFiLookAndFeel::getComboBoxFont (juce::ComboBox&) { return labelFont (13.0f); }

void LoFiLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 30, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centred);
}

void LoFiLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (Palette::panelHi);
    g.setColour (Palette::edge);
    g.drawRect (0, 0, width, height, 1);
}

juce::Font LoFiLookAndFeel::getPopupMenuFont() { return labelFont (13.0f); }

//==============================================================================
void LoFiLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    auto r = juce::Rectangle<float> (0, 0, (float) width, (float) height);
    g.setColour (Palette::ink);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (Palette::orange.withAlpha (0.7f));
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
    g.setColour (Palette::cream);
    g.setFont (valueFont (12.0f));
    g.drawFittedText (text, r.reduced (8, 4).toNearestInt(), juce::Justification::centredLeft, 4);
}

juce::Rectangle<int> LoFiLookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                                        juce::Rectangle<int> parentArea)
{
    const int w = juce::jmin (320, 16 + (int) (tipText.length() * 7.3f));
    const int lines = 1 + (int) (tipText.length() * 7.3f) / 300;
    const int h = 10 + 16 * lines;
    return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                 screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6)  : screenPos.y + 6,
                                 w, h).constrainedWithin (parentArea);
}
} // namespace lofi
