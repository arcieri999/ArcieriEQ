#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

using namespace juce;

class EQProcessor : public AudioProcessor
{
public:
    static constexpr int maxBands = 8;
    static constexpr int initialBands = 4;
    static constexpr int scopeSize = 4096;
    static constexpr int maxStages = 8;   // 8 biquads = 96 dB/oct

    enum FilterType { Peak = 0, LowShelf, HighShelf, LowPass, HighPass, Notch, BandPass };

    EQProcessor();
    ~EQProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (AudioBuffer<float>&, MidiBuffer&) override;

    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const String getName() const override { return "Arcieri EQ"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const String getProgramName (int) override { return {}; }
    void changeProgramName (int, const String&) override {}

    void getStateInformation (MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static StringArray getTypeNames()
    {
        return { "Peak", "Low Shelf", "High Shelf", "Low Pass", "High Pass", "Notch", "Band Pass" };
    }

    static StringArray getSlopeNames()
    {
        return { "6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct", "72 dB/oct", "96 dB/oct" };
    }

    // Devuelve cuantas etapas (biquads) se necesitan; LP/HP usan la pendiente, el resto usa 1 etapa
    static int makeStageCoeffs (int type, double sr, float freq, float q, float gainDb, int slope,
                                dsp::IIR::Coefficients<float>::Ptr* out);

    static dsp::IIR::Coefficients<float>::Ptr makeCoeffs (int type, double sr, float freq, float q, float gainDb);

    double getSampleRateHz() const { return currentSR; }

    AudioProcessorValueTreeState apvts;

    // Buffer circular (post-EQ) para el analizador de espectro
    float scopeBuf[scopeSize] {};
    std::atomic<int> scopePos { 0 };

private:
    static AudioProcessorValueTreeState::ParameterLayout createLayout();
    void updateFilters();

    struct Cache { float f = -1.f, g = -999.f, q = -1.f; int t = -1; int s = -1; };

    std::atomic<float>* pOn[maxBands] {};
    std::atomic<float>* pF[maxBands] {};
    std::atomic<float>* pG[maxBands] {};
    std::atomic<float>* pQ[maxBands] {};
    std::atomic<float>* pT[maxBands] {};
    std::atomic<float>* pS[maxBands] {};
    std::atomic<float>* pRange = nullptr;
    std::atomic<float>* pSolo = nullptr;

    Cache cache[maxBands];
    bool active[maxBands] {};
    dsp::IIR::Filter<float> filters[maxBands][2][maxStages];
    int numStages[maxBands] = { 1, 1, 1, 1, 1, 1, 1, 1 };
    bool soloUseBand = false;
    int soloBand = 0;
    dsp::IIR::Filter<float> soloFilters[2];
    bool soloActive = false;
    float soloF = -1.f, soloQ = -1.f;
    int soloT = -1;
    double currentSR = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQProcessor)
};
