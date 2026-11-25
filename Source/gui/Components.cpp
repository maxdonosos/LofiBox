#include "Components.h"

namespace lofi
{
using namespace Palette;

LoFiLookAndFeel& getLnf (juce::Component& c)
{
    if (auto* l = dynamic_cast<LoFiLookAndFeel*> (&c.getLookAndFeel())) return *l;
    static LoFiLookAndFeel fallback;
    return fallback;
}

//==============================================================================
//  Knob
//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& l)
    : label (l)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    slider.setMouseDragSensitivity (220);
    slider.setVelocityBasedMode (false);
    slider.addMouseListener (this, false);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, slider);

    if (auto* p = apvts.getParameter (paramId))
    {
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        slider.setPopupMenuEnabled (false);
        slider.setTooltip (p->getName (64) + "  (double-click: default)");
    }
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromBottom (20);
    const int s = juce::jmin (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (s, s));
}

void Knob::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().removeFromBottom (20).toFloat();
    const bool showValue = slider.isMouseOverOrDragging();
    auto& lnf = getLnf (*this);
    if (showValue)
    {
        g.setColour (orange);
        g.setFont (lnf.valueFont (11.5f));
        g.drawText (slider.getTextFromValue (slider.getValue()), area, juce::Justification::centred);
    }
    else
    {
        g.setColour (cream.withAlpha (isEnabled() ? 0.92f : 0.4f));
        g.setFont (lnf.labelFont (10.5f));
        g.drawText (label, area, juce::Justification::centred);
    }
}

//==============================================================================
//  DieButton
//==============================================================================
DieButton::DieButton() : rng (juce::Time::currentTimeMillis())
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip ("Randomize  (right-click: lock)");
}

void DieButton::setFace (int f, bool animate)
{
    face = juce::jlimit (1, 6, f);
    if (animate) { animFrames = 22; startTimerHz (60); }
    else { shownFace = face; angle = 0.0f; repaint(); }
}

void DieButton::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { if (lockable && onToggleLock) onToggleLock(); return; }
    if (locked) return;
    if (onRoll) onRoll();
}

void DieButton::timerCallback()
{
    if (--animFrames <= 0) { stopTimer(); shownFace = face; angle = 0.0f; repaint(); return; }
    const float t = (float) animFrames / 22.0f;
    angle = std::sin ((float) animFrames * 0.9f) * 0.6f * t;
    if (animFrames % 3 == 0) shownFace = 1 + rng.nextInt (6);
    repaint();
}

void DieButton::paint (juce::Graphics& g)
{
    const bool hover = isMouseOver();
    auto r = getLocalBounds().toFloat();
    const float s = juce::jmin (r.getWidth(), r.getHeight()) - 4.0f;
    auto die = juce::Rectangle<float> (s, s).withCentre (r.getCentre());
    const float corner = s * 0.22f;

    juce::Graphics::ScopedSaveState ss (g);
    g.addTransform (juce::AffineTransform::rotation (angle, die.getCentreX(), die.getCentreY()));

    juce::Path p; p.addRoundedRectangle (die, corner);
    juce::DropShadow (juce::Colours::black.withAlpha (0.6f), 6, { 0, 2 }).drawForPath (g, p);

    const auto body = locked ? juce::Colour (0xff4a4038) : (hover ? cream : juce::Colour (0xffe4d4b6));
    juce::ColourGradient grad (body.brighter (0.15f), die.getX(), die.getY(), body.darker (0.25f), die.getRight(), die.getBottom(), false);
    g.setGradientFill (grad);
    g.fillPath (p);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.strokePath (p, juce::PathStrokeType (1.0f));

    // pips
    const float pr = s * (big ? 0.085f : 0.095f);
    const float o = s * 0.27f;
    const auto c = die.getCentre();
    auto pip = [&] (float dx, float dy)
    {
        g.setColour (locked ? creamDim : (big ? rust : ink));
        g.fillEllipse (c.x + dx - pr, c.y + dy - pr, pr * 2, pr * 2);
    };
    switch (shownFace)
    {
        case 1: pip (0, 0); break;
        case 2: pip (-o, -o); pip (o, o); break;
        case 3: pip (-o, -o); pip (0, 0); pip (o, o); break;
        case 4: pip (-o, -o); pip (o, -o); pip (-o, o); pip (o, o); break;
        case 5: pip (-o, -o); pip (o, -o); pip (0, 0); pip (-o, o); pip (o, o); break;
        default: pip (-o, -o); pip (o, -o); pip (-o, 0); pip (o, 0); pip (-o, o); pip (o, o); break;
    }

    if (locked)
    {
        // candado mini
        const auto lr = juce::Rectangle<float> (s * 0.42f, s * 0.32f).withCentre (c.translated (0, s * 0.1f));
        g.setColour (orange);
        g.fillRoundedRectangle (lr, 1.5f);
        juce::Path shackle;
        shackle.addCentredArc (c.x, lr.getY(), s * 0.13f, s * 0.14f, 0, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
        g.strokePath (shackle, juce::PathStrokeType (1.8f));
    }
}

//==============================================================================
//  Botones momentáneos ligados a parámetros
//==============================================================================
MomentaryParamButton::MomentaryParamButton (juce::RangedAudioParameter& p)
    : attachment (p, [this] (float v) { paramDown = v > 0.5f; repaint(); }, nullptr)
{
    attachment.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void MomentaryParamButton::press (bool down)
{
    if (down == mouseIsDown) return;
    mouseIsDown = down;
    if (down) { attachment.beginGesture(); attachment.setValueAsPartOfGesture (1.0f); }
    else      { attachment.setValueAsPartOfGesture (0.0f); attachment.endGesture(); }
    repaint();
}

void MomentaryParamButton::mouseDown (const juce::MouseEvent& e) { if (! e.mods.isPopupMenu()) press (true); }
void MomentaryParamButton::mouseUp (const juce::MouseEvent&)     { press (false); }

void DropButton::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float d = juce::jmin (r.getWidth(), r.getHeight()) - 10.0f;
    auto circ = juce::Rectangle<float> (d, d).withCentre (r.getCentre());
    const bool down = isDown();
    const bool hover = isMouseOver();

    // bisel
    juce::Path outer; outer.addEllipse (circ);
    juce::DropShadow (juce::Colours::black.withAlpha (0.8f), 12, { 0, 4 }).drawForPath (g, outer);
    juce::ColourGradient bez (juce::Colour (0xff57493c), circ.getX(), circ.getY(), juce::Colour (0xff120e0b), circ.getRight(), circ.getBottom(), false);
    g.setGradientFill (bez);
    g.fillEllipse (circ);

    // halo de actividad
    if (activity > 0.01f)
    {
        g.setColour (orange.withAlpha (0.35f * activity));
        g.fillEllipse (circ.expanded (5.0f * activity));
    }

    auto cap = circ.reduced (d * 0.12f).translated (0, down ? 1.5f : 0.0f);
    if (down) cap = cap.reduced (1.5f);
    const auto top = down ? juce::Colour (0xff2c241d) : (hover ? juce::Colour (0xff3f342a) : juce::Colour (0xff362c24));
    juce::ColourGradient capG (top, cap.getCentreX(), cap.getY(), juce::Colour (0xff17120e), cap.getCentreX(), cap.getBottom(), false);
    g.setGradientFill (capG);
    g.fillEllipse (cap);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (cap, 1.0f);

    // LED de actividad arriba
    const auto led = juce::Rectangle<float> (7, 7).withCentre ({ cap.getCentreX(), cap.getY() + cap.getHeight() * 0.24f });
    g.setColour (activity > 0.02f ? orangeHot.interpolatedWith (red, 1.0f - activity) : juce::Colour (0xff3a2e25));
    g.fillEllipse (led);

    g.setColour (down ? orange : cream);
    g.setFont (getLnf (*this).titleFont (d * 0.2f));
    g.drawText ("DROP", cap.withTrimmedTop (cap.getHeight() * 0.18f), juce::Justification::centred);
}

void TapeStopButton::setVisualState (int tapeState, float sp, bool externallyHeld, bool toggleMode)
{
    const float target = (isDown() || externallyHeld) ? 1.0f : 0.0f;
    const float newAnim = pressAnim + (target - pressAnim) * 0.45f;
    if (tapeState != state || std::abs (sp - speed) > 0.002f || externallyHeld != extHeld
        || toggleMode != toggle || std::abs (newAnim - pressAnim) > 0.001f)
    {
        state = tapeState; speed = sp; extHeld = externallyHeld; toggle = toggleMode; pressAnim = newAnim;
        repaint();
    }
}

void TapeStopButton::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float D = juce::jmin (r.getWidth(), r.getHeight()) - 40.0f;   // margen para el halo
    const auto c = r.getCentre();
    auto outer = juce::Rectangle<float> (D, D).withCentre (c);
    const bool engaged = state != 0;                // Stopping, Stopped, Starting, CatchUp
    const float glow = engaged ? (state == 2 ? 1.0f : 0.55f + 0.45f * (1.0f - speed)) : 0.0f;
    auto& lnf = getLnf (*this);

    // --- halo
    if (glow > 0.0f)
        for (int i = 4; i >= 1; --i)
        {
            g.setColour (orange.withAlpha (0.07f * glow * (float) (5 - i)));
            g.fillEllipse (outer.expanded ((float) i * 4.0f));
        }

    // --- base / bisel metálico
    juce::Path base; base.addEllipse (outer);
    juce::DropShadow (juce::Colours::black.withAlpha (0.9f), 24, { 0, 8 }).drawForPath (g, base);
    {
        juce::ColourGradient grad (juce::Colour (0xff6b5a4a), outer.getX(), outer.getY(),
                                   juce::Colour (0xff100c09), outer.getRight(), outer.getBottom(), false);
        grad.addColour (0.5, juce::Colour (0xff2c241d));
        g.setGradientFill (grad);
        g.fillEllipse (outer);
    }
    // anillo de ticks alrededor (como escala de velocidad)
    {
        const float rr = D * 0.5f;
        const int n = 48;
        for (int i = 0; i < n; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / (float) n;
            const bool lit = engaged && (float) i / (float) n > speed;
            g.setColour (lit ? orangeHot.withAlpha (0.95f) : juce::Colour (0x55efe2c8));
            g.drawLine ({ c.getPointOnCircumference (rr * 0.885f, a), c.getPointOnCircumference (rr * (i % 4 == 0 ? 0.80f : 0.83f), a) },
                        i % 4 == 0 ? 2.0f : 1.2f);
        }
    }
    // pozo interior
    auto well = outer.reduced (D * 0.17f);
    {
        juce::ColourGradient wellG (juce::Colour (0xff0b0806), well.getCentreX(), well.getY(),
                                    juce::Colour (0xff2a211a), well.getCentreX(), well.getBottom(), false);
        g.setGradientFill (wellG);
        g.fillEllipse (well);
    }

    // --- tapa de goma naranja (se hunde al apretar)
    const float press = pressAnim;
    auto cap = well.reduced (D * 0.035f + press * 3.0f).translated (0, press * 2.5f);
    {
        juce::Path capP; capP.addEllipse (cap);
        juce::DropShadow (juce::Colours::black.withAlpha (0.85f - 0.4f * press), (int) (14 - 8 * press), { 0, (int) (6 - 4 * press) }).drawForPath (g, capP);

        const auto hi = engaged ? orangeHot : orange;
        juce::ColourGradient capG (hi.brighter (0.15f - 0.25f * press), cap.getCentreX() - cap.getWidth() * 0.25f, cap.getY(),
                                   rust.darker (0.35f + 0.3f * press), cap.getCentreX() + cap.getWidth() * 0.2f, cap.getBottom(), true);
        g.setGradientFill (capG);
        g.fillEllipse (cap);

        // textura de goma: anillos concéntricos muy sutiles
        g.setColour (juce::Colours::black.withAlpha (0.06f));
        for (float k = 0.25f; k < 1.0f; k += 0.12f)
            g.drawEllipse (cap.reduced (cap.getWidth() * 0.5f * k), 1.0f);

        // brillo especular
        juce::ColourGradient spec (juce::Colour (0x55ffffff).withMultipliedAlpha (1.0f - 0.7f * press),
                                   cap.getCentreX(), cap.getY(), juce::Colour (0x00ffffff), cap.getCentreX(), cap.getCentreY(), false);
        g.setGradientFill (spec);
        g.fillEllipse (cap.reduced (cap.getWidth() * 0.12f, 0).withHeight (cap.getHeight() * 0.55f).translated (0, cap.getHeight() * 0.03f));

        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawEllipse (cap, 1.2f);
    }

    // --- textos
    {
        const float h = cap.getHeight(), cy = cap.getCentreY();
        g.setColour (ink.withAlpha (0.88f));
        g.setFont (lnf.titleFont (h * 0.21f));
        g.drawText ("TAPE", juce::Rectangle<float> (cap.getX(), cy - h * 0.27f, cap.getWidth(), h * 0.24f), juce::Justification::centred);
        g.drawText ("STOP", juce::Rectangle<float> (cap.getX(), cy - h * 0.04f, cap.getWidth(), h * 0.24f), juce::Justification::centred);
        g.setColour (ink.withAlpha (0.35f));
        g.fillRect (juce::Rectangle<float> (h * 0.22f, 1.2f).withCentre ({ cap.getCentreX(), cy - h * 0.035f }));
    }

    const char* status = "";
    switch (state)
    {
        case 1: status = "BRAKING"; break;
        case 2: status = "STOPPED"; break;
        case 3: case 4: status = "SPIN-UP"; break;
        default: status = toggle ? "TAP" : "HOLD"; break;
    }
    g.setFont (lnf.labelFont (cap.getHeight() * 0.075f));
    g.setColour (ink.withAlpha (0.65f));
    g.drawText (status, juce::Rectangle<float> (cap.getX(), cap.getCentreY() + cap.getHeight() * 0.22f, cap.getWidth(), cap.getHeight() * 0.1f), juce::Justification::centred);
}

//==============================================================================
//  SegmentedChoice
//==============================================================================
SegmentedChoice::SegmentedChoice (juce::RangedAudioParameter& p, juce::StringArray l, bool rev)
    : labels (std::move (l)), reversed (rev),
      attachment (p, [this] (float v)
                  {
                      int idx = juce::roundToInt (v);
                      if (reversed) idx = labels.size() - 1 - idx;
                      selected = juce::jlimit (0, labels.size() - 1, idx);
                      if (onChange) onChange (selected);
                      repaint();
                  }, nullptr)
{
    attachment.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void SegmentedChoice::mouseDown (const juce::MouseEvent& e)
{
    const int n = labels.size();
    const int idx = juce::jlimit (0, n - 1, (int) ((float) e.x / (float) getWidth() * (float) n));
    attachment.setValueAsCompleteGesture ((float) (reversed ? n - 1 - idx : idx));
}

void SegmentedChoice::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const float cr = r.getHeight() * 0.5f;
    g.setColour (ink);
    g.fillRoundedRectangle (r, cr);
    g.setColour (edge);
    g.drawRoundedRectangle (r, cr, 1.0f);

    const int n = labels.size();
    const float w = r.getWidth() / (float) n;
    auto sel = juce::Rectangle<float> (r.getX() + w * (float) selected, r.getY(), w, r.getHeight()).reduced (2.0f);
    juce::ColourGradient grad (orangeHot, sel.getX(), sel.getY(), rust, sel.getX(), sel.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (sel, sel.getHeight() * 0.5f);

    g.setFont (getLnf (*this).labelFont (juce::jmin (11.0f, r.getHeight() * 0.46f)));
    for (int i = 0; i < n; ++i)
    {
        g.setColour (i == selected ? ink : creamDim);
        g.drawText (labels[i], juce::Rectangle<float> (r.getX() + w * (float) i, r.getY(), w, r.getHeight()), juce::Justification::centred);
    }
}

//==============================================================================
//  VU meter
//==============================================================================
void VUMeter::update (float rms, float peak, float dt)
{
    const float dbfs = juce::Decibels::gainToDecibels (rms, -100.0f);
    const float vu   = juce::jlimit (-22.0f, 4.0f, dbfs + 18.0f);     // 0 VU = -18 dBFS
    const float coeff = 1.0f - std::exp (-dt / 0.065f);               // ~300 ms de integración (99%)
    const float newNeedle = needleVu + (vu - needleVu) * coeff;
    peakHold = peak > 0.89f ? 1.0f : juce::jmax (0.0f, peakHold - dt * 2.5f);
    if (std::abs (newNeedle - needleVu) > 0.01f || peakHold > 0.0f) { needleVu = newNeedle; repaint(); }
    else needleVu = newNeedle;
}

void VUMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto& lnf = getLnf (*this);

    // marco
    g.setColour (juce::Colour (0xff0d0a08));
    g.fillRoundedRectangle (r, 8.0f);
    auto face = r.reduced (5.0f);

    // cara crema con viñeta cálida
    {
        juce::ColourGradient grad (juce::Colour (0xfff6e8c9), face.getCentreX(), face.getBottom(),
                                   juce::Colour (0xffcbb48a), face.getCentreX(), face.getY() - face.getHeight() * 0.4f, true);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (face, 5.0f);
    }

    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (face.toNearestInt());

    const float R = juce::jmin (face.getWidth() * 0.50f, face.getHeight() * 0.92f);
    const juce::Point<float> pivot { face.getCentreX(), face.getBottom() + face.getHeight() * 0.24f };
    const float a0 = juce::degreesToRadians (-48.0f), a1 = juce::degreesToRadians (48.0f);
    auto vuToAngle = [&] (float vu)
    {
        // escala VU clásica: no lineal (comprimida abajo)
        const float lin = std::pow (10.0f, vu / 20.0f);
        const float t = juce::jlimit (0.0f, 1.0f, (lin - 0.1f) / (1.4125f - 0.1f));
        return a0 + t * (a1 - a0);
    };

    // arco de escala + zona roja
    juce::Path arc;
    arc.addCentredArc (pivot.x, pivot.y, R, R, 0, vuToAngle (-20.0f), vuToAngle (0.0f), true);
    g.setColour (ink);
    g.strokePath (arc, juce::PathStrokeType (1.4f));
    juce::Path red;
    red.addCentredArc (pivot.x, pivot.y, R, R, 0, vuToAngle (0.0f), vuToAngle (3.0f), true);
    g.setColour (Palette::red);
    g.strokePath (red, juce::PathStrokeType (4.0f));

    const int marks[] = { -20, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3 };
    g.setFont (lnf.labelFont (9.0f));
    for (int m : marks)
    {
        const float a = vuToAngle ((float) m);
        const bool lbl = (m == -20 || m == -10 || m == -7 || m == -5 || m == -3 || m == 0 || m == 3);
        g.setColour (m > 0 ? Palette::red : ink);
        g.drawLine ({ pivot.getPointOnCircumference (R, a), pivot.getPointOnCircumference (R + (lbl ? 9.0f : 5.0f), a) }, 1.2f);
        if (lbl)
        {
            const auto tp = pivot.getPointOnCircumference (R + 17.0f, a);
            g.drawText (m > 0 ? "+" + juce::String (m) : juce::String (std::abs (m)),
                        juce::Rectangle<float> (24, 12).withCentre (tp), juce::Justification::centred);
        }
    }

    g.setColour (ink.withAlpha (0.85f));
    g.setFont (lnf.titleFont (18.0f));
    g.drawText ("VU", face.withTrimmedTop (face.getHeight() * 0.46f).withHeight (20), juce::Justification::centred);
    g.setFont (lnf.labelFont (7.5f));
    g.setColour (ink.withAlpha (0.5f));
    g.drawText ("LO-FI BOX", face.withTrimmedTop (face.getHeight() * 0.64f).withHeight (12), juce::Justification::centred);

    // aguja
    const float na = vuToAngle (needleVu);
    const auto tip = pivot.getPointOnCircumference (R + 6.0f, na);
    const auto from = pivot.getPointOnCircumference (R * 0.30f, na);
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.drawLine ({ from.translated (2.5f, 2.0f), tip.translated (2.5f, 2.0f) }, 1.6f);
    g.setColour (ink);
    g.drawLine ({ from, tip }, 1.5f);
    g.setColour (Palette::rust);
    g.drawLine ({ pivot.getPointOnCircumference (R - 10.0f, na), tip }, 1.6f);

    // LED de pico
    const auto led = juce::Rectangle<float> (7, 7).withPosition (face.getRight() - 13.0f, face.getY() + 6.0f);
    g.setColour (peakHold > 0.0f ? Palette::red.withAlpha (0.4f + 0.6f * peakHold) : juce::Colour (0xff8a7a62));
    g.fillEllipse (led);
    g.setColour (ink.withAlpha (0.5f));
    g.setFont (lnf.labelFont (7.0f));
    g.drawText ("PEAK", led.translated (-30.0f, -2.5f).withWidth (28).withHeight (12), juce::Justification::centredRight);

    // sombra interior superior (vidrio)
    juce::ColourGradient glass (juce::Colour (0x40000000), face.getX(), face.getY(), juce::Colour (0x00000000), face.getX(), face.getY() + 18.0f, false);
    g.setGradientFill (glass);
    g.fillRect (face);
}

//==============================================================================
//  Cassette
//==============================================================================
void Cassette::advance (float speed, float dt)
{
    lastSpeed = speed;
    angle += speed * dt * juce::MathConstants<float>::twoPi * 0.55f;   // ~0.55 vueltas/seg
    angle = std::fmod (angle, juce::MathConstants<float>::twoPi * 6.0f);
    repaint();
}

void Cassette::drawReel (juce::Graphics& g, juce::Point<float> c, float hubR, float packR, float a)
{
    // pack de cinta (marrón óxido)
    juce::ColourGradient tape (juce::Colour (0xff5a3624), c.x, c.y, juce::Colour (0xff2a170e), c.x + packR, c.y, true);
    g.setGradientFill (tape);
    g.fillEllipse (c.x - packR, c.y - packR, packR * 2, packR * 2);
    g.setColour (juce::Colour (0x18ffffff));
    for (float k = hubR * 1.2f; k < packR; k += 3.0f)
        g.drawEllipse (c.x - k, c.y - k, k * 2, k * 2, 0.5f);

    // hub blanco con dientes
    g.setColour (juce::Colour (0xffe9dcc2));
    g.fillEllipse (c.x - hubR, c.y - hubR, hubR * 2, hubR * 2);
    g.setColour (juce::Colour (0xff1a1410));
    g.fillEllipse (c.x - hubR * 0.55f, c.y - hubR * 0.55f, hubR * 1.1f, hubR * 1.1f);
    for (int i = 0; i < 6; ++i)
    {
        const float ang = a + juce::MathConstants<float>::twoPi * (float) i / 6.0f;
        juce::Path tooth;
        tooth.addRectangle (-hubR * 0.09f, -hubR * 0.58f, hubR * 0.18f, hubR * 0.22f);
        g.setColour (juce::Colour (0xffe9dcc2));
        g.fillPath (tooth, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
    }
    // marca de rotación visible
    g.setColour (Palette::orange);
    const auto m = c.getPointOnCircumference (hubR * 0.78f, a);
    g.fillEllipse (m.x - 1.6f, m.y - 1.6f, 3.2f, 3.2f);
}

void Cassette::paint (juce::Graphics& g)
{
    auto& lnf = getLnf (*this);
    auto r = getLocalBounds().toFloat().reduced (4.0f);

    // carcasa
    juce::Path shell; shell.addRoundedRectangle (r, 10.0f);
    juce::DropShadow (juce::Colours::black.withAlpha (0.85f), 18, { 0, 7 }).drawForPath (g, shell);
    juce::ColourGradient sg (juce::Colour (0xff3a3029), r.getX(), r.getY(), juce::Colour (0xff1b1511), r.getRight(), r.getBottom(), false);
    g.setGradientFill (sg);
    g.fillPath (shell);
    g.setColour (juce::Colour (0x30ffffff));
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

    // tornillos
    auto screw = [&] (float x, float y)
    {
        g.setColour (juce::Colour (0xff0e0b09)); g.fillEllipse (x - 4, y - 4, 8, 8);
        g.setColour (juce::Colour (0xff7a6a58)); g.fillEllipse (x - 3, y - 3, 6, 6);
        g.setColour (juce::Colour (0xff221a14)); g.drawLine (x - 2.2f, y - 2.2f, x + 2.2f, y + 2.2f, 1.2f);
    };
    screw (r.getX() + 10, r.getY() + 10); screw (r.getRight() - 10, r.getY() + 10);
    screw (r.getX() + 10, r.getBottom() - 10); screw (r.getRight() - 10, r.getBottom() - 10);

    // etiqueta de papel
    auto label = r.reduced (r.getWidth() * 0.065f, 0).withTrimmedTop (r.getHeight() * 0.075f).withHeight (r.getHeight() * 0.66f);
    g.setColour (juce::Colour (0xffe8d9bb));
    g.fillRoundedRectangle (label, 5.0f);

    // franjas retro
    {
        auto stripes = label.withTrimmedTop (label.getHeight() * 0.20f).withHeight (label.getHeight() * 0.11f);
        const juce::Colour cols[] = { Palette::orangeHot, Palette::orange, Palette::rust, Palette::brown };
        const float h = stripes.getHeight() / 4.0f;
        for (int i = 0; i < 4; ++i)
        {
            g.setColour (cols[i]);
            g.fillRect (stripes.getX(), stripes.getY() + h * (float) i, stripes.getWidth(), h + 0.5f);
        }
    }

    g.setColour (Palette::ink);
    g.setFont (lnf.titleFont (label.getHeight() * 0.15f));
    g.drawText ("LO-FI BOX", label.withHeight (label.getHeight() * 0.20f).reduced (10, 0), juce::Justification::centredLeft);
    g.setFont (lnf.labelFont (label.getHeight() * 0.075f));
    g.drawText ("SIDE A   C-60", label.withHeight (label.getHeight() * 0.20f).reduced (10, 0), juce::Justification::centredRight);

    // ventana
    auto win = label.withTrimmedTop (label.getHeight() * 0.38f).withTrimmedBottom (label.getHeight() * 0.09f)
                    .reduced (label.getWidth() * 0.12f, 0);
    g.setColour (juce::Colour (0xff0c0907));
    g.fillRoundedRectangle (win, win.getHeight() * 0.5f);

    const float hubR = win.getHeight() * 0.30f;
    const juce::Point<float> lc { win.getX() + win.getWidth() * 0.22f, win.getCentreY() };
    const juce::Point<float> rc { win.getRight() - win.getWidth() * 0.22f, win.getCentreY() };

    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path wp; wp.addRoundedRectangle (win, win.getHeight() * 0.5f);
        g.reduceClipRegion (wp);
        // cinta entre carretes
        g.setColour (juce::Colour (0xff3b2317));
        g.fillRect (juce::Rectangle<float> (lc.x, win.getBottom() - 5.0f, rc.x - lc.x, 3.0f));
        // el carrete izquierdo (suministro) más lleno; las velocidades angulares compensan
        drawReel (g, lc, hubR, win.getHeight() * 0.74f, angle * 0.72f);
        drawReel (g, rc, hubR, win.getHeight() * 0.52f, angle);
        // reflejo del vidrio
        juce::ColourGradient glass (juce::Colour (0x22ffffff), win.getX(), win.getY(), juce::Colour (0x00ffffff), win.getX(), win.getCentreY(), false);
        g.setGradientFill (glass);
        g.fillRect (win);
    }
    g.setColour (juce::Colour (0x80000000));
    g.drawRoundedRectangle (win, win.getHeight() * 0.5f, 1.5f);

    // trapecio inferior (cabeza)
    juce::Path trap;
    const float tw = r.getWidth() * 0.62f, th = r.getHeight() * 0.18f;
    const float tx = r.getCentreX(), ty = r.getBottom();
    trap.startNewSubPath (tx - tw * 0.5f, ty);
    trap.lineTo (tx - tw * 0.42f, ty - th);
    trap.lineTo (tx + tw * 0.42f, ty - th);
    trap.lineTo (tx + tw * 0.5f, ty);
    trap.closeSubPath();
    g.setColour (juce::Colour (0xff2a221b));
    g.fillPath (trap);
    g.setColour (juce::Colour (0x25ffffff));
    g.strokePath (trap, juce::PathStrokeType (1.0f));
    for (float hx : { -0.26f, 0.26f })
    {
        g.setColour (juce::Colour (0xff0b0806));
        g.fillEllipse (tx + tw * hx - 4.0f, ty - th * 0.55f - 4.0f, 8.0f, 8.0f);
    }
    // indicador de velocidad
    g.setColour (lastSpeed < 0.98f ? Palette::orangeHot : Palette::creamFaint);
    g.setFont (lnf.labelFont (9.0f));
    g.drawText (juce::String (juce::roundToInt (juce::jlimit (0.0f, 1.0f, lastSpeed) * 100.0f)) + "% SPEED",
                juce::Rectangle<float> (tx - 50.0f, ty - th * 0.75f, 100.0f, 12.0f), juce::Justification::centred);
}

//==============================================================================
//  ModulePanel
//==============================================================================
ModulePanel::ModulePanel (const juce::String& n, const juce::String& t) : number (n), title (t)
{
}

void ModulePanel::attachPower (juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    hasPower = true;
    addAndMakeVisible (power);
    power.setTooltip ("Module on / off");
    power.onStateChange = [this] { setContentEnabledLook (power.getToggleState()); };
    powerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, id, power);
    setContentEnabledLook (power.getToggleState());
}

void ModulePanel::setContentEnabledLook (bool active)
{
    for (auto* c : contentComps) c->setAlpha (active ? 1.0f : 0.38f);
    repaint();
}

void ModulePanel::resized()
{
    auto head = getLocalBounds().removeFromTop (44).reduced (10, 10);
    if (hasDie) die.setBounds (head.removeFromRight (26).withSizeKeepingCentre (26, 26));
    head.removeFromRight (6);
    if (hasPower) power.setBounds (head.removeFromRight (48));
}

void ModulePanel::paint (juce::Graphics& g)
{
    auto& lnf = getLnf (*this);
    auto r = getLocalBounds().toFloat().reduced (1.0f);

    juce::Path card; card.addRoundedRectangle (r, 12.0f);
    juce::DropShadow (juce::Colours::black.withAlpha (0.55f), 16, { 0, 5 }).drawForPath (g, card);

    juce::ColourGradient grad (Palette::panelHi, r.getX(), r.getY(), Palette::panel.darker (0.2f), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillPath (card);

    // filete superior iluminado + borde
    g.setColour (juce::Colour (0x14ffffff));
    g.drawLine (r.getX() + 12, r.getY() + 1.0f, r.getRight() - 12, r.getY() + 1.0f, 1.0f);
    g.setColour (Palette::edge.withAlpha (0.9f));
    g.drawRoundedRectangle (r, 12.0f, 1.0f);

    // cabecera
    auto head = r.removeFromTop (44.0f).reduced (14.0f, 0);
    g.setColour (Palette::orange);
    g.setFont (lnf.labelFont (11.0f));
    g.drawText (number, head.removeFromLeft (24.0f), juce::Justification::centredLeft);
    g.setColour (Palette::cream);
    g.setFont (lnf.titleFont (19.0f));
    g.drawText (title, head, juce::Justification::centredLeft);

    // línea separadora perforada
    const float y = 44.0f;
    g.setColour (juce::Colour (0xff120e0b));
    for (float x = 14.0f; x < (float) getWidth() - 14.0f; x += 6.0f)
        g.fillRect (x, y, 3.0f, 1.0f);
}

void Caption::paint (juce::Graphics& g)
{
    g.setColour (colour);
    g.setFont (getLnf (*this).labelFont (height));
    g.drawText (text, getLocalBounds(), just);
}
} // namespace lofi
