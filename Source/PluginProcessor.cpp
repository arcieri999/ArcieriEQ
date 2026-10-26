#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "License.h"

EQProcessor::EQProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    for (int i = 0; i < maxBands; ++i)
    {
        auto n = String (i + 1);
        pOn[i] = apvts.getRawParameterValue ("on" + n);
        pF[i]  = apvts.getRawParameterValue ("f" + n);
        pG[i]  = apvts.getRawParameterValue ("g" + n);
        pQ[i]  = apvts.getRawParameterValue ("q" + n);
        pT[i]  = apvts.getRawParameterValue ("t" + n);
        pS[i]  = apvts.getRawParameterValue ("s" + n);
    }
    pRange = apvts.getRawParameterValue ("range");
    pSolo  = apvts.getRawParameterValue ("solo");

    licensed.store (License::isLicensed());
}

AudioProcessorValueTreeState::ParameterLayout EQProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    const float defF[maxBands] = { 80.f, 400.f, 2500.f, 8000.f, 150.f, 1000.f, 5000.f, 12000.f };
    const int   defT[maxBands] = { LowShelf, Peak, Peak, HighShelf, Peak, Peak, Peak, Peak };

    for (int i = 0; i < maxBands; ++i)
    {
        auto n = String (i + 1);
        p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "on" + n, 1 }, "Band " + n + " On", i < initialBands));
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "f" + n, 1 }, "Band " + n + " Freq",
                         NormalisableRange<float> (20.f, 20000.f, 0.1f, 0.3f), defF[i]));
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "g" + n, 1 }, "Band " + n + " Gain",
                         NormalisableRange<float> (-48.f, 48.f, 0.01f), 0.f));
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "q" + n, 1 }, "Band " + n + " Q",
                         NormalisableRange<float> (0.1f, 18.f, 0.01f, 0.4f), (defT[i] == Peak ? 1.0f : 0.71f)));
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "t" + n, 1 }, "Band " + n + " Type",
                         getTypeNames(), defT[i]));
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "s" + n, 1 }, "Band " + n + " Slope",
                         getSlopeNames(), 1));
    }

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "range", 1 }, "Range (dB)",
                     NormalisableRange<float> (3.f, 48.f, 1.f), 18.f));

    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "solo", 1 }, "Solo Band", 0, 8, 0));

    return { p.begin(), p.end() };
}

int EQProcessor::makeStageCoeffs (int type, double sr, float freq, float q, float gainDb, int slope,
                                  dsp::IIR::Coefficients<float>::Ptr* out)
{
    using C = dsp::IIR::Coefficients<float>;

    if (type != LowPass && type != HighPass)
    {
        out[0] = makeCoeffs (type, sr, freq, q, gainDb);
        return 1;
    }

    static const int orders[8] = { 1, 2, 3, 4, 6, 8, 12, 16 };
    const int order = orders[jlimit (0, 7, slope)];
    const bool lp = (type == LowPass);
    freq = jlimit (20.f, (float) (sr * 0.49), freq);

    int n = 0;
    if (order & 1)
        out[n++] = lp ? C::makeFirstOrderLowPass (sr, freq) : C::makeFirstOrderHighPass (sr, freq);

    const int pairs = order / 2;
    for (int k = 0; k < pairs; ++k)
    {
        const float theta = (order & 1) ? (float) (k + 1) * MathConstants<float>::pi / (float) order
                                        : (float) (2 * k + 1) * MathConstants<float>::pi / (float) (2 * order);
        float qk = 1.0f / (2.0f * std::cos (theta));       // Butterworth
        if (k == pairs - 1)
            qk *= q / 0.7071f;                              // el Q del usuario actua como resonancia
        qk = jmax (0.1f, qk);
        out[n++] = lp ? C::makeLowPass (sr, freq, qk) : C::makeHighPass (sr, freq, qk);
    }
    return n;
}

dsp::IIR::Coefficients<float>::Ptr EQProcessor::makeCoeffs (int type, double sr, float freq, float q, float gainDb)
{
    using C = dsp::IIR::Coefficients<float>;
    freq = jlimit (20.f, (float) (sr * 0.49), freq);
    q = jmax (0.05f, q);
    auto gain = Decibels::decibelsToGain (gainDb);

    switch (type)
    {
        case LowShelf:  return C::makeLowShelf  (sr, freq, q, gain);
        case HighShelf: return C::makeHighShelf (sr, freq, q, gain);
        case LowPass:   return C::makeLowPass   (sr, freq, q);
        case HighPass:  return C::makeHighPass  (sr, freq, q);
        case Notch:     return C::makeNotch     (sr, freq, q);
        case BandPass:  return C::makeBandPass  (sr, freq, q);
        default:        return C::makePeakFilter (sr, freq, q, gain);
    }
}

void EQProcessor::prepareToPlay (double sampleRate, int)
{
    currentSR = sampleRate;
    for (int i = 0; i < maxBands; ++i)
    {
        cache[i] = Cache();
        for (auto& chn : filters[i]) for (auto& f : chn) f.reset();
    }
    soloF = -1.f;
    for (auto& f : soloFilters) f.reset();
    updateFilters();
}

bool EQProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    auto out = l.getMainOutputChannelSet();
    return (out == AudioChannelSet::mono() || out == AudioChannelSet::stereo()) && out == l.getMainInputChannelSet();
}

void EQProcessor::updateFilters()
{
    const float range = pRange->load();

    for (int i = 0; i < maxBands; ++i)
    {
        active[i] = pOn[i]->load() > 0.5f;
        if (! active[i]) continue;

        const float f = pF[i]->load();
        const float g = jlimit (-range, range, pG[i]->load());
        const float q = pQ[i]->load();
        const int   t = (int) pT[i]->load();
        const int   s = (int) pS[i]->load();

        if (f != cache[i].f || g != cache[i].g || q != cache[i].q || t != cache[i].t || s != cache[i].s)
        {
            cache[i] = { f, g, q, t, s };
            dsp::IIR::Coefficients<float>::Ptr cs[maxStages];
            const int cnt = makeStageCoeffs (t, currentSR, f, q, g, s, cs);
            numStages[i] = cnt;
            for (int k = 0; k < cnt; ++k)
            {
                filters[i][0][k].coefficients = cs[k];
                filters[i][1][k].coefficients = cs[k];
            }
        }
    }

    // Solo: aisla la zona de frecuencias de una sola banda
    const int sb = (int) pSolo->load() - 1;
    soloActive = (sb >= 0 && sb < maxBands && active[sb]);
    soloBand = jlimit (0, maxBands - 1, sb);
    {
        const int st = (int) pT[soloBand]->load();
        soloUseBand = soloActive && (st == LowPass || st == HighPass);
    }
    if (soloActive && ! soloUseBand)
    {
        const float f = pF[sb]->load();
        const float q = pQ[sb]->load();
        const int   t = (int) pT[sb]->load();

        if (f != soloF || q != soloQ || t != soloT)
        {
            soloF = f; soloQ = q; soloT = t;
            int iso = BandPass;                              // Peak, Notch, Band Pass
            if (t == LowShelf  || t == LowPass)  iso = LowPass;
            if (t == HighShelf || t == HighPass) iso = HighPass;
            auto c = makeCoeffs (iso, currentSR, f, q, 0.f);
            soloFilters[0].coefficients = c;
            soloFilters[1].coefficients = c;
        }
    }
}

void EQProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    if (! licensed.load())      // sin licencia: el audio pasa sin ecualizar
        return;
    updateFilters();

    const int numCh = jmin (buffer.getNumChannels(), 2);
    const int n = buffer.getNumSamples();

    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int s = 0; s < n; ++s)
        {
            float x = d[s];
            if (soloActive)
            {
                if (soloUseBand)
                    for (int k = 0; k < numStages[soloBand]; ++k)
                        x = filters[soloBand][ch][k].processSample (x);
                else
                    x = soloFilters[ch].processSample (x);
            }
            else
            {
                for (int b = 0; b < maxBands; ++b)
                    if (active[b])
                        for (int k = 0; k < numStages[b]; ++k)
                            x = filters[b][ch][k].processSample (x);
            }
            d[s] = x;
        }
    }

    // Alimentar el analizador con la señal ya ecualizada (mono)
    if (numCh > 0)
    {
        int pos = scopePos.load (std::memory_order_relaxed);
        for (int s = 0; s < n; ++s)
        {
            float m = buffer.getSample (0, s);
            if (numCh > 1) m = 0.5f * (m + buffer.getSample (1, s));
            scopeBuf[pos] = m;
            pos = (pos + 1) & (scopeSize - 1);
        }
        scopePos.store (pos, std::memory_order_relaxed);
    }
}

AudioProcessorEditor* EQProcessor::createEditor() { return new EQEditor (*this); }

void EQProcessor::getStateInformation (MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void EQProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new EQProcessor(); }
