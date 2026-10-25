// Teste de host do DSP (sem Win32): FFT, bandas e escala.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "dsp/band_plan.h"
#include "dsp/spectrum.h"

namespace {

int gFailures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FALHOU: %s\n", message);
        ++gFailures;
    }
}

void testBandPlan() {
    const auto c15 = dsp::BandPlan::centers(15);
    check(c15.size() == 15, "15 centros");
    check(std::fabs(c15.front() - 25.0f) < 0.001f, "primeiro centro = 25 Hz");
    check(std::fabs(c15.back() - 16000.0f) < 0.001f, "ultimo centro = 16 kHz");

    const auto e15 = dsp::BandPlan::edges(c15);
    check(e15.size() == 16, "16 bordas");
    for (size_t i = 1; i < e15.size(); ++i) {
        check(e15[i] > e15[i - 1], "bordas monotonas");
    }

    check(dsp::BandPlan::label(25.0f) == L"25", "label 25");
    check(dsp::BandPlan::label(1000.0f) == L"1k", "label 1k");
    check(dsp::BandPlan::label(2500.0f) == L"2,5k", "label 2,5k");
    check(dsp::BandPlan::label(16000.0f) == L"16k", "label 16k");

    check(dsp::BandPlan::clampBands(1) == 3, "clamp minimo");
    check(dsp::BandPlan::clampBands(99) == 19, "clamp maximo");
}

void testSine() {
    constexpr int rate = 48000;
    constexpr int total = rate;   // 1 segundo
    std::vector<float> samples(static_cast<size_t>(total));
    for (int i = 0; i < total; ++i) {
        samples[static_cast<size_t>(i)] =
            0.5f * std::sin(2.0f * 3.14159265358979323846f * 1000.0f * i /
                            rate);
    }

    dsp::SpectrumAnalyzer analyzer(rate, 15);
    std::vector<float> levels;
    bool produced = false;
    for (int pos = 0; pos < total; pos += 480) {
        const int n = std::min(480, total - pos);
        if (analyzer.feed(samples.data() + pos, static_cast<size_t>(n),
                          levels)) {
            produced = true;
        }
    }

    check(produced, "seno produz quadro de FFT");
    check(levels.size() == 15, "15 niveis");
    if (levels.size() == 15) {
        const auto maxIt = std::max_element(levels.begin(), levels.end());
        const int maxBand = static_cast<int>(maxIt - levels.begin());
        std::printf("seno 1 kHz: banda=%d nivel=%.3f\n", maxBand,
                    static_cast<double>(*maxIt));
        check(maxBand == 8, "1 kHz cai na banda 8");
        check(*maxIt > 0.5f, "nivel do seno > 0,5");
        check(levels.front() < *maxIt, "25 Hz menor que o pico");
        check(levels.back() < *maxIt, "16 kHz menor que o pico");
    }

    // Silencio: todos os níveis devem voltar a zero.
    analyzer.reset(rate);
    std::vector<float> silence(static_cast<size_t>(480), 0.0f);
    for (int i = 0; i < 120; ++i) {
        analyzer.feed(silence.data(), silence.size(), levels);
    }
    float maxSilence = 0.0f;
    for (float v : levels) {
        maxSilence = std::max(maxSilence, v);
    }
    check(maxSilence < 0.001f, "silencio zera os niveis");
}

}  // namespace

int main() {
    testBandPlan();
    testSine();
    if (gFailures == 0) {
        std::printf("OK: testes de DSP passaram\n");
        return EXIT_SUCCESS;
    }
    std::fprintf(stderr, "%d teste(s) falharam\n", gFailures);
    return EXIT_FAILURE;
}
