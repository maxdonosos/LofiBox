#pragma once
//==============================================================================
//  Componentes custom de la GUI de Lo-Fi Box
//==============================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include "LoFiLookAndFeel.h"

namespace lofi
{
LoFiLookAndFeel& getLnf (juce::Component&);

//==============================================================================
/** Knob con label debajo; muestra el valor al pasar el mouse / arrastrar. */
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& label);
    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { repaint(); }

    juce::Slider slider;
private:
    juce::String label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

//==============================================================================
/** Dado: click = randomizar, click derecho = candado (excluir del dado global). */
class DieButton : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    DieButton();
    std::function<void()> onRoll, onToggleLock;

    void setFace (int f, bool animate);
    void setLocked (bool l)        { if (locked != l) { locked = l; repaint(); } }
    void setLockable (bool l)      { lockable = l; }
    void setBig (bool b)           { big = b; }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { repaint(); }

private:
    void timerCallback() override;
    int face = 5, shownFace = 5, animFrames = 0;
    float angle = 0.0f;
    bool locked = false, lockable = true, big = false;
    juce::Random rng;
};

//==============================================================================
/** Botón momentáneo ligado a un parámetro bool (down = 1, up = 0) con gestos. */
class MomentaryParamButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    MomentaryParamButton (juce::RangedAudioParameter& p);
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { repaint(); }
    void press (bool down);
    bool isDown() const noexcept { return paramDown || mouseIsDown; }

protected:
    bool mouseIsDown = false, paramDown = false;
private:
    juce::ParameterAttachment attachment;
};

/** Botón DROP (dropouts). */
class DropButton : public MomentaryParamButton
{
public:
    using MomentaryParamButton::MomentaryParamButton;
    void setActivity (float a) { if (std::abs (a - activity) > 0.01f) { activity = a; repaint(); } }
    void paint (juce::Graphics&) override;
private:
    float activity = 0.0f;
};

/** El protagonista: botón TAPE STOP grande y táctil. */
class TapeStopButton : public MomentaryParamButton
{
public:
    using MomentaryParamButton::MomentaryParamButton;
    void setVisualState (int tapeState, float speed, bool externallyHeld, bool toggleMode);
    void paint (juce::Graphics&) override;
private:
    int state = 0;
    float speed = 1.0f, pressAnim = 0.0f;
    bool extHeld = false, toggle = false;
};

//==============================================================================
/** Selector segmentado para parámetros choice/bool. */
class SegmentedChoice : public juce::Component
{
public:
    SegmentedChoice (juce::RangedAudioParameter& p, juce::StringArray labels, bool reversed = false);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    int getSelected() const noexcept { return selected; }
    std::function<void (int)> onChange;
private:
    juce::StringArray labels;
    bool reversed;
    int selected = 0;
    juce::ParameterAttachment attachment;
};

//==============================================================================
/** VU meter analógico con aguja (0 VU = -18 dBFS RMS) y LED de pico. */
class VUMeter : public juce::Component
{
public:
    void update (float rmsLinear, float peakLinear, float dtSeconds);
    void paint (juce::Graphics&) override;
private:
    float needleVu = -20.0f, peakHold = 0.0f;
};

//==============================================================================
/** Cassette con carretes que giran según la velocidad real de la cinta. */
class Cassette : public juce::Component
{
public:
    void advance (float speed, float dtSeconds);
    void paint (juce::Graphics&) override;
private:
    void drawReel (juce::Graphics&, juce::Point<float> c, float hubR, float packR, float angle);
    float angle = 0.0f, lastSpeed = 1.0f;
};

//==============================================================================
/** Panel de módulo: tarjeta con número, título, LED de bypass y dado. */
class ModulePanel : public juce::Component
{
public:
    ModulePanel (const juce::String& number, const juce::String& title);
    void paint (juce::Graphics&) override;
    void resized() override;

    /** área disponible para contenido, en coordenadas locales */
    juce::Rectangle<int> content() const { return getLocalBounds().withTrimmedTop (44).reduced (8, 6); }

    void attachPower (juce::AudioProcessorValueTreeState&, const juce::String& paramId);
    void setContentEnabledLook (bool active);
    void addContent (juce::Component& c) { addAndMakeVisible (c); contentComps.add (&c); }

    juce::ToggleButton power;
    DieButton die;
    bool hasPower = false, hasDie = false;
private:
    juce::String number, title;
    juce::Array<juce::Component*> contentComps;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment;
};

/** Caption pequeño (texto fijo) */
class Caption : public juce::Component
{
public:
    explicit Caption (juce::String t, juce::Justification j = juce::Justification::centredLeft) : text (std::move (t)), just (j) { setInterceptsMouseClicks (false, false); }
    void setText (const juce::String& t) { if (t != text) { text = t; repaint(); } }
    void paint (juce::Graphics& g) override;
    juce::Colour colour = Palette::creamDim;
    float height = 10.0f;
private:
    juce::String text;
    juce::Justification just;
};
} // namespace lofi
