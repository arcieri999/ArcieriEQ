#include "PluginEditor.h"
#include <BinaryData.h>
#include "License.h"

static const float minF = 20.f, maxF = 20000.f;

EQEditor::EQEditor (EQProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    setSize (1180, 580);

    logo = ImageCache::getFromMemory (BinaryData::logo_png, BinaryData::logo_pngSize);

    auto setupKnob = [this] (Slider& s, Label& l, const String& name, const String& suffix)
    {
        s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (Slider::TextBoxBelow, false, 80, 18);
        s.setTextValueSuffix (suffix);
        s.setNumDecimalPlacesToDisplay (2);
        addAndMakeVisible (s);
        l.setText (name, dontSendNotification);
        l.setJustificationType (Justification::centred);
        addAndMakeVisible (l);
    };
    setupKnob (freqS, freqL, "FREQ", " Hz");
    setupKnob (gainS, gainL, "GAIN", " dB");
    setupKnob (qS, qL, "Q", "");
    setupKnob (rangeS, rangeL, "RANGO", " dB");
    freqS.setNumDecimalPlacesToDisplay (1);
    rangeS.setNumDecimalPlacesToDisplay (0);

    typeBox.addItemList (EQProcessor::getTypeNames(), 1);
    addAndMakeVisible (typeBox);
    typeL.setText ("TIPO", dontSendNotification);
    typeL.setJustificationType (Justification::centred);
    addAndMakeVisible (typeL);

    slopeBox.addItemList (EQProcessor::getSlopeNames(), 1);
    addAndMakeVisible (slopeBox);
    slopeL.setText ("PENDIENTE", dontSendNotification);
    slopeL.setJustificationType (Justification::centred);
    addAndMakeVisible (slopeL);

    addAndMakeVisible (addBtn);
    addAndMakeVisible (delBtn);
    addAndMakeVisible (soloBtn);
    soloBtn.onClick = [this]
    {
        int cur = (int) getParam ("solo");
        setParam ("solo", cur == selected + 1 ? 0.f : (float) (selected + 1));
    };

    for (int i = 0; i < EQProcessor::maxBands; ++i)
    {
        auto& sl = bandS[i];
        sl.setSliderStyle (Slider::LinearVertical);
        sl.setTextBoxStyle (Slider::TextBoxBelow, false, 36, 14);
        sl.setNumDecimalPlacesToDisplay (1);
        sl.setDoubleClickReturnValue (true, 0.0);
        sl.onDragStart = [this, i] { selectBand (i); };
        addAndMakeVisible (sl);
        aB[i] = std::make_unique<SA> (proc.apvts, "g" + String (i + 1), sl);
    }
    addBtn.onClick = [this]
    {
        for (int i = 0; i < EQProcessor::maxBands; ++i)
            if (! isActive (i))
            {
                addBandAt ({ graph.getCentreX(), graph.getCentreY() });
                return;
            }
    };
    delBtn.onClick = [this] { removeBand (selected); };

    aR = std::make_unique<SA> (proc.apvts, "range", rangeS);
    rangeS.onValueChange = [this]
    {
        updateGainRanges();
        repaint();
    };

    selectBand (0);
    startTimerHz (30);

    if (! proc.isLicensed())
    {
        auto safe = Component::SafePointer<EQEditor> (this);
        activation = std::make_unique<ActivationPanel> (proc, logo, [safe]
        {
            MessageManager::callAsync ([safe]
            {
                if (safe != nullptr) { safe->activation.reset(); safe->repaint(); }
            });
        });
        addAndMakeVisible (*activation);
        activation->setBounds (getLocalBounds());
    }
}

EQEditor::~EQEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

// ----------------------------------------------------------------- helpers
bool EQEditor::isActive (int i) const { return getParam ("on" + String (i + 1)) > 0.5f; }

float EQEditor::getParam (const String& id) const { return *proc.apvts.getRawParameterValue (id); }

void EQEditor::setParam (const String& id, float v)
{
    if (auto* p = proc.apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (v));
}

void EQEditor::setParamGesture (const String& id, bool begin)
{
    if (auto* p = proc.apvts.getParameter (id))
    {
        if (begin) p->beginChangeGesture(); else p->endChangeGesture();
    }
}

float EQEditor::range() const { return getParam ("range"); }

// Frecuencias graves a la IZQUIERDA, agudas a la DERECHA
float EQEditor::freqToX (float f) const
{
    float p = std::log (jlimit (minF, maxF, f) / minF) / std::log (maxF / minF);
    return graph.getX() + p * graph.getWidth();
}

float EQEditor::xToFreq (float x) const
{
    float p = jlimit (0.f, 1.f, (x - graph.getX()) / graph.getWidth());
    return minF * std::pow (maxF / minF, p);
}

float EQEditor::gainToY (float g) const { return graph.getCentreY() - (g / range()) * graph.getHeight() * 0.5f; }
float EQEditor::yToGain (float y) const { return (graph.getCentreY() - y) / (graph.getHeight() * 0.5f) * range(); }

void EQEditor::updateGainRanges()
{
    gainS.setRange (-range(), range(), 0.1);
    for (auto& b : bandS) b.setRange (-range(), range(), 0.1);
}

void EQEditor::selectBand (int i)
{
    selected = i;
    auto n = String (i + 1);
    aF.reset(); aG.reset(); aQ.reset(); aT.reset(); aS.reset();
    aF = std::make_unique<SA> (proc.apvts, "f" + n, freqS);
    aG = std::make_unique<SA> (proc.apvts, "g" + n, gainS);
    aQ = std::make_unique<SA> (proc.apvts, "q" + n, qS);
    aT = std::make_unique<CA> (proc.apvts, "t" + n, typeBox);
    aS = std::make_unique<CA> (proc.apvts, "s" + n, slopeBox);
    updateGainRanges();
    repaint();
}

void EQEditor::addBandAt (Point<float> p)
{
    for (int i = 0; i < EQProcessor::maxBands; ++i)
    {
        if (isActive (i)) continue;
        auto n = String (i + 1);
        setParam ("on" + n, 1.f);
        setParam ("t" + n, (float) EQProcessor::Peak);
        setParam ("f" + n, xToFreq (p.x));
        setParam ("g" + n, jlimit (-range(), range(), yToGain (p.y)));
        setParam ("q" + n, 1.0f);
        setParam ("s" + n, 1.f);
        selectBand (i);
        return;
    }
}

void EQEditor::removeBand (int i)
{
    setParam ("on" + String (i + 1), 0.f);
    for (int k = 0; k < EQProcessor::maxBands; ++k)
        if (isActive (k)) { selectBand (k); return; }
    repaint();
}

int EQEditor::hitNode (Point<float> p) const
{
    int best = -1;
    float bestD = 14.f;
    for (int i = 0; i < EQProcessor::maxBands; ++i)
    {
        if (! isActive (i)) continue;
        auto n = String (i + 1);
        int t = (int) getParam ("t" + n);
        float gy = (t == EQProcessor::LowPass || t == EQProcessor::HighPass || t == EQProcessor::Notch || t == EQProcessor::BandPass)
                       ? graph.getCentreY() : gainToY (getParam ("g" + n));
        float d = p.getDistanceFrom ({ freqToX (getParam ("f" + n)), gy });
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

void EQEditor::showNodeMenu (int i)
{
    PopupMenu m;
    auto names = EQProcessor::getTypeNames();
    int cur = (int) getParam ("t" + String (i + 1));
    for (int k = 0; k < names.size(); ++k)
        m.addItem (k + 1, names[k], true, k == cur);
    m.addSeparator();
    m.addItem (100, "Eliminar banda");

    m.showMenuAsync (PopupMenu::Options(), [this, i] (int r)
    {
        if (r == 100) removeBand (i);
        else if (r > 0) setParam ("t" + String (i + 1), (float) (r - 1));
    });
}

// ------------------------------------------------------------------- mouse
void EQEditor::mouseDown (const MouseEvent& e)
{
    if (! graph.contains (e.position)) return;
    int h = hitNode (e.position);
    if (h < 0) return;

    selectBand (h);
    if (e.mods.isPopupMenu()) { showNodeMenu (h); return; }

    dragging = h;
    auto n = String (h + 1);
    setParamGesture ("f" + n, true);
    setParamGesture ("g" + n, true);
}

void EQEditor::mouseDrag (const MouseEvent& e)
{
    if (dragging < 0) return;
    auto n = String (dragging + 1);
    int t = (int) getParam ("t" + n);

    setParam ("f" + n, xToFreq (jlimit (graph.getX(), graph.getRight(), e.position.x)));

    bool usesGain = (t == EQProcessor::Peak || t == EQProcessor::LowShelf || t == EQProcessor::HighShelf);
    if (usesGain)
        setParam ("g" + n, jlimit (-range(), range(), yToGain (e.position.y)));
}

void EQEditor::mouseUp (const MouseEvent&)
{
    if (dragging < 0) return;
    auto n = String (dragging + 1);
    setParamGesture ("f" + n, false);
    setParamGesture ("g" + n, false);
    dragging = -1;
}

void EQEditor::mouseDoubleClick (const MouseEvent& e)
{
    if (graph.contains (e.position) && hitNode (e.position) < 0)
        addBandAt (e.position);
}

void EQEditor::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    int h = hitNode (e.position);
    if (h < 0) h = selected;
    auto id = "q" + String (h + 1);
    float q = getParam (id) * std::exp (w.deltaY * 0.8f);
    setParam (id, jlimit (0.1f, 18.f, q));
}

// ------------------------------------------------------------------- timer
void EQEditor::timerCallback()
{
    soloBtn.setToggleState ((int) getParam ("solo") == selected + 1, dontSendNotification);
    for (int i = 0; i < EQProcessor::maxBands; ++i)
    {
        bool on = isActive (i);
        bool usesGain = (int) getParam ("t" + String (i + 1)) <= EQProcessor::HighShelf;
        bandS[i].setEnabled (on && usesGain);
        bandS[i].setAlpha (on ? 1.f : 0.35f);
    }

    {
        const int selT = (int) getParam ("t" + String (selected + 1));
        const bool lphp = (selT == EQProcessor::LowPass || selT == EQProcessor::HighPass);
        slopeBox.setEnabled (lphp);
        slopeBox.setAlpha (lphp ? 1.f : 0.35f);
    }

    const int pos = proc.scopePos.load (std::memory_order_relaxed);
    for (int i = 0; i < fftSize; ++i)
        fftData[i] = proc.scopeBuf[(pos - fftSize + i + EQProcessor::scopeSize) & (EQProcessor::scopeSize - 1)];
    zeromem (fftData + fftSize, sizeof (float) * fftSize);

    window.multiplyWithWindowingTable (fftData, (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData);

    const double sr = jmax (8000.0, proc.getSampleRateHz());
    const float refDb = Decibels::gainToDecibels ((float) fftSize * 0.25f);

    for (int c = 0; c < numCols; ++c)
    {
        float f0 = minF * std::pow (maxF / minF, (float) c / numCols);
        float f1 = minF * std::pow (maxF / minF, (float) (c + 1) / numCols);
        int b0 = jlimit (1, fftSize / 2 - 1, (int) std::floor (f0 * fftSize / sr));
        int b1 = jlimit (b0, fftSize / 2 - 1, (int) std::ceil  (f1 * fftSize / sr));

        float mx = 0.f;
        for (int b = b0; b <= b1; ++b) mx = jmax (mx, fftData[b]);

        float db = Decibels::gainToDecibels (mx, -120.f) - refDb;
        float norm = jlimit (0.f, 1.f, (db + 80.f) / 80.f);
        levels[c] = jmax (norm, levels[c] * 0.88f);   // caída suave
    }
    repaint (graph.toNearestInt());
}

// ------------------------------------------------------------------ drawing
void EQEditor::resized()
{
    if (activation != nullptr) activation->setBounds (getLocalBounds());

    auto r = getLocalBounds();
    r.removeFromTop (64);
    auto bottom = r.removeFromBottom (140);
    sidePanel = r.removeFromRight (310).reduced (6, 8);
    graph = r.reduced (16, 8).toFloat();
    graph.removeFromLeft (30);   // etiquetas dB
    graph.removeFromBottom (18); // etiquetas de frecuencia

    // sliders de ganancia (derecha)
    auto sp = sidePanel.reduced (8, 0);
    sp.removeFromTop (44);
    const int cw = sp.getWidth() / EQProcessor::maxBands;
    for (int i = 0; i < EQProcessor::maxBands; ++i)
        bandS[i].setBounds (sp.removeFromLeft (cw));

    // controles inferiores
    auto b2 = bottom.reduced (16, 6);
    b2.removeFromTop (16);
    auto knob = [&b2] (int w) { auto a = b2.removeFromLeft (w); b2.removeFromLeft (8); return a; };

    auto t = knob (240);
    const int tx = t.getX(), ty = t.getY();
    typeL.setBounds  (tx, ty - 16, 116, 18);
    typeBox.setBounds (tx, ty + 8, 116, 28);
    slopeL.setBounds  (tx + 124, ty - 16, 116, 18);
    slopeBox.setBounds (tx + 124, ty + 8, 116, 28);
    addBtn.setBounds  (tx,       ty + 50, 74, 26);
    delBtn.setBounds  (tx + 80,  ty + 50, 74, 26);
    soloBtn.setBounds (tx + 160, ty + 50, 74, 26);

    for (auto& pr : std::initializer_list<std::pair<Slider*, Label*>> { { &freqS, &freqL }, { &gainS, &gainL }, { &qS, &qL } })
    {
        auto a = knob (110);
        pr.second->setBounds (a.removeFromTop (16).translated (0, -16));
        pr.first->setBounds (a.translated (0, -6));
    }

    auto rg = bottom.reduced (16, 6).removeFromRight (110);
    rg.removeFromTop (16);
    rangeL.setBounds (rg.removeFromTop (16).translated (0, -16));
    rangeS.setBounds (rg.translated (0, -6));
}

void EQEditor::drawSpectrum (Graphics& g)
{
    for (int c = 0; c < numCols; ++c)
    {
        float f0 = minF * std::pow (maxF / minF, (float) c / numCols);
        float f1 = minF * std::pow (maxF / minF, (float) (c + 1) / numCols);
        float xa = freqToX (f0), xb = freqToX (f1);
        float lv = levels[c];
        if (lv < 0.01f) continue;

        float h = lv * graph.getHeight();
        auto bar = Rectangle<float> (xa + 0.5f, graph.getBottom() - h, xb - xa - 1.f, h);

        // la intensidad del morado depende del nivel (ganancia) de esa frecuencia
        auto col = Pal::purple.withAlpha (0.08f + 0.85f * lv * lv);
        ColourGradient gr (col, bar.getX(), bar.getY(), Pal::deep.withAlpha (0.05f), bar.getX(), graph.getBottom(), false);
        g.setGradientFill (gr);
        g.fillRect (bar);

        g.setColour (Colours::white.withAlpha (0.55f * lv * lv));
        g.fillRect (bar.withHeight (1.5f));
    }
}

void EQEditor::drawCurve (Graphics& g)
{
    const double sr = proc.getSampleRateHz();
    std::vector<dsp::IIR::Coefficients<float>::Ptr> cs;
    for (int i = 0; i < EQProcessor::maxBands; ++i)
    {
        if (! isActive (i)) continue;
        auto n = String (i + 1);
        dsp::IIR::Coefficients<float>::Ptr tmp[EQProcessor::maxStages];
        const int cnt = EQProcessor::makeStageCoeffs ((int) getParam ("t" + n), sr, getParam ("f" + n), getParam ("q" + n),
                                                      jlimit (-range(), range(), getParam ("g" + n)),
                                                      (int) getParam ("s" + n), tmp);
        for (int k = 0; k < cnt; ++k) cs.push_back (tmp[k]);
    }

    Path line;
    bool first = true;
    for (float x = graph.getX(); x <= graph.getRight(); x += 2.f)
    {
        float f = xToFreq (x);
        float db = 0.f;
        for (auto& c : cs) db += Decibels::gainToDecibels ((float) c->getMagnitudeForFrequency (f, sr), -100.f);
        float y = jlimit (graph.getY(), graph.getBottom(), gainToY (db));
        if (first) { line.startNewSubPath (x, y); first = false; } else line.lineTo (x, y);
    }

    Path fill (line);
    fill.lineTo (graph.getRight(), graph.getCentreY());
    fill.lineTo (graph.getX(), graph.getCentreY());
    fill.closeSubPath();
    g.setColour (Pal::purple.withAlpha (0.12f));
    g.fillPath (fill);

    g.setColour (Pal::purple.withAlpha (0.25f));
    g.strokePath (line, PathStrokeType (6.f));
    g.setColour (Colours::white.withAlpha (0.95f));
    g.strokePath (line, PathStrokeType (1.8f));
}

void EQEditor::drawNodes (Graphics& g)
{
    for (int i = 0; i < EQProcessor::maxBands; ++i)
    {
        if (! isActive (i)) continue;
        auto n = String (i + 1);
        int t = (int) getParam ("t" + n);
        bool flat = (t == EQProcessor::LowPass || t == EQProcessor::HighPass || t == EQProcessor::Notch || t == EQProcessor::BandPass);
        float x = freqToX (getParam ("f" + n));
        float y = flat ? graph.getCentreY() : gainToY (jlimit (-range(), range(), getParam ("g" + n)));

        bool sel = (i == selected);
        auto c = Rectangle<float> (22.f, 22.f).withCentre ({ x, y });
        if ((int) getParam ("solo") == i + 1)
        {
            g.setColour (Colours::white);
            g.drawEllipse (c.expanded (9.f), 2.f);
        }
        g.setColour (Pal::purple.withAlpha (sel ? 0.45f : 0.2f));
        g.fillEllipse (c.expanded (5.f));
        g.setColour (sel ? Colours::white : Pal::purple);
        g.fillEllipse (c);
        g.setColour (sel ? Colours::black : Colours::white);
        g.setFont (FontOptions (13.f, Font::bold));
        g.drawText (n, c.toNearestInt(), Justification::centred);
    }
}

void EQEditor::paint (Graphics& g)
{
    g.fillAll (Pal::bg);

    // Cabecera con logo
    g.setColour (Pal::panel);
    g.fillRect (0, 0, getWidth(), 64);
    g.setColour (Pal::deep);
    g.fillRect (0, 63, getWidth(), 1);
    g.drawImageWithin (logo, 16, 8, 48, 48, RectanglePlacement::centred);
    g.setColour (Pal::text);
    g.setFont (FontOptions (22.f, Font::bold));
    g.drawText ("ARCIERI EQ", 76, 0, 300, 64, Justification::centredLeft);
    g.setFont (FontOptions (12.f));
    g.setColour (Pal::text.withAlpha (0.55f));
    g.drawText ("Doble clic: agregar banda  |  Rueda: Q  |  Clic derecho: tipo / eliminar",
                getWidth() - 520, 0, 504, 64, Justification::centredRight);

    // Fondo de la gráfica
    g.setColour (Pal::panel);
    g.fillRoundedRectangle (graph, 6.f);

    // Rejilla de dB
    g.setFont (FontOptions (11.f));
    for (int k = -3; k <= 3; ++k)
    {
        float db = (float) k * range() / 3.f;
        float y = gainToY (db);
        g.setColour (k == 0 ? Pal::deep : Pal::grid);
        g.drawHorizontalLine ((int) y, graph.getX(), graph.getRight());
        g.setColour (Pal::text.withAlpha (0.6f));
        g.drawText (String (db, 1), (int) graph.getX() - 34, (int) y - 7, 32, 14, Justification::centredRight);
    }

    // Rejilla de frecuencias (altas a la izquierda, bajas a la derecha)
    const float fl[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    for (float f : fl)
    {
        float x = freqToX (f);
        g.setColour (Pal::grid);
        g.drawVerticalLine ((int) x, graph.getY(), graph.getBottom());
        g.setColour (Pal::text.withAlpha (0.6f));
        String s = f >= 1000 ? String (f / 1000, f >= 10000 ? 0 : 0) + "k" : String ((int) f);
        g.drawText (s, (int) x - 20, (int) graph.getBottom() + 2, 40, 14, Justification::centred);
    }

    g.saveState();
    g.reduceClipRegion (graph.toNearestInt());
    drawSpectrum (g);
    drawCurve (g);
    g.restoreState();
    drawNodes (g);

    g.setColour (Pal::deep);
    g.drawRoundedRectangle (graph, 6.f, 1.f);

    // Panel de sliders de ganancia
    g.setColour (Pal::panel);
    g.fillRoundedRectangle (sidePanel.toFloat(), 6.f);
    g.setColour (Pal::deep);
    g.drawRoundedRectangle (sidePanel.toFloat(), 6.f, 1.f);
    g.setColour (Pal::text);
    g.setFont (FontOptions (12.f, Font::bold));
    g.drawText ("GANANCIA POR BANDA", sidePanel.getX(), sidePanel.getY() + 4, sidePanel.getWidth(), 16, Justification::centred);
    {
        auto sp = sidePanel.reduced (8, 0);
        sp.removeFromTop (44);
        const int cw = sp.getWidth() / EQProcessor::maxBands;
        for (int i = 0; i < EQProcessor::maxBands; ++i)
        {
            auto col = sp.removeFromLeft (cw);
            g.setColour (i == selected ? Pal::purple : Pal::text.withAlpha (isActive (i) ? 0.9f : 0.35f));
            g.drawText (String (i + 1), col.getX(), sidePanel.getY() + 24, cw, 18, Justification::centred);
        }
    }

    // Etiqueta de banda seleccionada
    g.setColour (Pal::purple);
    g.setFont (FontOptions (13.f, Font::bold));
    g.drawText ("BANDA " + String (selected + 1), 16, getHeight() - 140, 120, 18, Justification::centredLeft);
}


// ------------------------------------------------------------ Activacion
ActivationPanel::ActivationPanel (EQProcessor& p, Image logoImg, std::function<void()> onOk)
    : proc (p), logo (std::move (logoImg)), onActivated (std::move (onOk)), code (License::machineCode())
{
    keyBox.setMultiLine (true, true);
    keyBox.setReturnKeyStartsNewLine (false);
    keyBox.setTextToShowWhenEmpty ("Pega aquí tu licencia", Pal::text.withAlpha (0.4f));
    keyBox.setColour (TextEditor::backgroundColourId, Pal::panel);
    keyBox.setColour (TextEditor::textColourId, Pal::text);
    keyBox.setColour (TextEditor::outlineColourId, Pal::deep);
    keyBox.setColour (TextEditor::focusedOutlineColourId, Pal::purple);
    addAndMakeVisible (keyBox);

    activateBtn.setColour (TextButton::buttonColourId, Pal::deep);
    activateBtn.setColour (TextButton::textColourOffId, Colours::white);
    addAndMakeVisible (activateBtn);
    addAndMakeVisible (copyBtn);

    status.setJustificationType (Justification::centred);
    status.setColour (Label::textColourId, Pal::text);
    addAndMakeVisible (status);

    copyBtn.onClick = [this]
    {
        SystemClipboard::copyTextToClipboard (code);
        status.setText ("Código copiado. Envíaselo a Arcieri.", dontSendNotification);
    };

    activateBtn.onClick = [this]
    {
        if (License::activate (keyBox.getText()))
        {
            proc.setLicensed (true);
            status.setText ("¡Activado!", dontSendNotification);
            if (onActivated) onActivated();
        }
        else
        {
            status.setText ("Licencia inválida para este equipo. Revisa que la pegaste completa.", dontSendNotification);
        }
    };
}

void ActivationPanel::paint (Graphics& g)
{
    g.fillAll (Pal::bg);
    auto c = area();

    g.drawImageWithin (logo, c.getCentreX() - 50, c.getY(), 100, 100, RectanglePlacement::centred);

    g.setColour (Pal::text);
    g.setFont (FontOptions (26.f, Font::bold));
    g.drawText ("ARCIERI EQ", c.getX(), c.getY() + 104, c.getWidth(), 34, Justification::centred);

    g.setFont (FontOptions (14.f));
    g.setColour (Pal::text.withAlpha (0.75f));
    g.drawText ("Activa tu copia. Envía este código de equipo a Arcieri para recibir tu licencia:",
                c.getX(), c.getY() + 146, c.getWidth(), 20, Justification::centred);

    g.setColour (Pal::purple);
    g.setFont (FontOptions (28.f, Font::bold));
    g.drawText (code, c.getX(), c.getY() + 172, c.getWidth(), 40, Justification::centred);
}

void ActivationPanel::resized()
{
    auto c = area();
    copyBtn.setBounds (c.getCentreX() - 70, c.getY() + 216, 140, 28);
    keyBox.setBounds (c.getX(), c.getY() + 256, c.getWidth(), 80);
    activateBtn.setBounds (c.getCentreX() - 80, c.getY() + 348, 160, 34);
    status.setBounds (c.getX(), c.getY() + 392, c.getWidth(), 24);
}
