#include "PluginProcessor.h"
#include "PluginEditor.h"

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
    }
    pRange = apvts.getRawParameterValue ("range");
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
    }

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "range", 1 }, "Range (dB)",
                     NormalisableRange<float> (3.f, 48.f, 1.f), 18.f));

    return { p.begin(), p.end() };
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
        for (auto& f : filters[i]) f.reset();
    }
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

        if (f != cache[i].f || g != cache[i].g || q != cache[i].q || t != cache[i].t)
        {
            cache[i] = { f, g, q, t };
            auto c = makeCoeffs (t, currentSR, f, q, g);
            filters[i][0].coefficients = c;
            filters[i][1].coefficients = c;
        }
    }
}

void EQProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    updateFilters();

    const int numCh = jmin (buffer.getNumChannels(), 2);
    const int n = buffer.getNumSamples();

    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int s = 0; s < n; ++s)
        {
            float x = d[s];
            for (int b = 0; b < maxBands; ++b)
                if (active[b])
                    x = filters[b][ch].processSample (x);
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
