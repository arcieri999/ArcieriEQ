#pragma once
#include "PluginProcessor.h"

namespace Pal
{
    const Colour bg     { 0xff050507 };
    const Colour panel  { 0xff0e0b14 };
    const Colour grid   { 0xff241a35 };
    const Colour purple { 0xffa24dff };
    const Colour deep   { 0xff6a1bd0 };
    const Colour text   { 0xffd9c8f5 };
}

class PurpleLNF : public LookAndFeel_V4
{
public:
    PurpleLNF()
    {
        setColour (Slider::rotarySliderFillColourId, Pal::purple);
        setColour (Slider::rotarySliderOutlineColourId, Pal::grid);
        setColour (Slider::thumbColourId, Colours::white);
        setColour (Slider::trackColourId, Pal::purple);
        setColour (Slider::backgroundColourId, Pal::grid);
        setColour (TextButton::buttonOnColourId, Pal::purple);
        setColour (TextButton::textColourOnId, Colours::white);
        setColour (Slider::textBoxTextColourId, Pal::text);
        setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
        setColour (Label::textColourId, Pal::text);
        setColour (ComboBox::backgroundColourId, Pal::panel);
        setColour (ComboBox::textColourId, Pal::text);
        setColour (ComboBox::outlineColourId, Pal::deep);
        setColour (ComboBox::arrowColourId, Pal::purple);
        setColour (PopupMenu::backgroundColourId, Pal::panel);
        setColour (PopupMenu::textColourId, Pal::text);
        setColour (PopupMenu::highlightedBackgroundColourId, Pal::deep);
        setColour (TextButton::buttonColourId, Pal::panel);
        setColour (TextButton::textColourOffId, Pal::text);
    }
};

class ActivationPanel : public Component
{
public:
    ActivationPanel (EQProcessor& p, Image logoImg, std::function<void()> onOk);
    void paint (Graphics&) override;
    void resized() override;

private:
    Rectangle<int> area() const { return getLocalBounds().withSizeKeepingCentre (640, 470); }

    EQProcessor& proc;
    Image logo;
    std::function<void()> onActivated;
    String code;
    TextEditor keyBox;
    TextButton activateBtn { "ACTIVAR" }, copyBtn { "Copiar código" };
    Label status;
};

class EQEditor : public AudioProcessorEditor, private Timer
{
public:
    explicit EQEditor (EQProcessor&);
    ~EQEditor() override;

    void paint (Graphics&) override;
    void resized() override;
    void mouseDown (const MouseEvent&) override;
    void mouseDrag (const MouseEvent&) override;
    void mouseUp (const MouseEvent&) override;
    void mouseDoubleClick (const MouseEvent&) override;
    void mouseWheelMove (const MouseEvent&, const MouseWheelDetails&) override;

private:
    using SA = AudioProcessorValueTreeState::SliderAttachment;
    using CA = AudioProcessorValueTreeState::ComboBoxAttachment;

    void timerCallback() override;
    void selectBand (int i);
    void updateGainRanges();
    void addBandAt (Point<float> p);
    void removeBand (int i);
    void showNodeMenu (int i);
    int  hitNode (Point<float> p) const;
    bool isActive (int i) const;
    void setParam (const String& id, float v);
    void setParamGesture (const String& id, bool begin);
    float getParam (const String& id) const;

    float freqToX (float f) const;
    float xToFreq (float x) const;
    float gainToY (float g) const;
    float yToGain (float y) const;
    float range() const;

    void drawSpectrum (Graphics&);
    void drawCurve (Graphics&);
    void drawNodes (Graphics&);

    EQProcessor& proc;
    PurpleLNF lnf;
    std::unique_ptr<ActivationPanel> activation;
    Image logo;
    Rectangle<float> graph;

    int selected = 0;
    int dragging = -1;

    Slider freqS, gainS, qS, rangeS;
    ComboBox typeBox, slopeBox;
    TextButton addBtn { "+ Banda" }, delBtn { "Eliminar" }, soloBtn { "SOLO" };
    Slider bandS[EQProcessor::maxBands];
    Rectangle<int> sidePanel;
    Label freqL, gainL, qL, rangeL, typeL, slopeL;

    std::unique_ptr<SA> aF, aG, aQ, aR;
    std::unique_ptr<SA> aB[EQProcessor::maxBands];
    std::unique_ptr<CA> aT, aS;

    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int numCols = 120;
    dsp::FFT fft { fftOrder };
    dsp::WindowingFunction<float> window { (size_t) fftSize, dsp::WindowingFunction<float>::hann };
    float fftData[fftSize * 2] {};
    float levels[numCols] {};   // 0..1 por columna de frecuencia

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQEditor)
};
