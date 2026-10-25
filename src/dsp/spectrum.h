// Análise espectral do graph-eq: FFT 2048 com janela Hann, hop de 10 ms,
// bandas logarítmicas e escala em dB com tilt de +2,5 dB/oitava.
#pragma once
#include <cstddef>
#include <vector>

namespace dsp {

class SpectrumAnalyzer {
public:
    static constexpr int kFftSize = 2048;
    static constexpr float kMinDb = -72.0f;
    static constexpr float kMaxDb = -6.0f;
    static constexpr float kTiltDbPerOctave = 2.5f;

    explicit SpectrumAnalyzer(int sampleRate = 48000, int bands = 15);

    void reset(int sampleRate);
    void setBandCount(int bands);
    int bandCount() const { return bands_; }
    int sampleRate() const { return sampleRate_; }

    // Consome amostras mono (-1..1). Devolve true quando pelo menos um quadro
    // de FFT foi produzido; nesse caso `levels` tem `bandCount()` valores 0..1.
    bool feed(const float* mono, std::size_t frames, std::vector<float>& levels);

private:
    void analyze(std::vector<float>& levels);
    void dropBacklog();

    int sampleRate_;
    int bands_;
    int hop_;
    bool primed_ = false;
    std::vector<float> hann_;
    std::vector<float> window_;
    std::vector<float> pending_;
    std::vector<float> re_;
    std::vector<float> im_;
    std::vector<float> magnitudes_;
    std::vector<float> centers_;
    std::vector<float> edges_;
};

}  // namespace dsp
