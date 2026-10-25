#include "dsp/spectrum.h"

#include <algorithm>
#include <cmath>

#include "dsp/band_plan.h"
#include "dsp/fft.h"

namespace dsp {

namespace {
constexpr double kPi = 3.14159265358979323846;

int hopFor(int sampleRate) {
    const int rate = std::max(1, sampleRate);
    return std::max(1, std::min(SpectrumAnalyzer::kFftSize,
                                static_cast<int>(std::lround(rate / 100.0))));
}
}  // namespace

SpectrumAnalyzer::SpectrumAnalyzer(int sampleRate, int bands)
    : sampleRate_(sampleRate > 0 ? sampleRate : 48000),
      bands_(BandPlan::clampBands(bands)),
      hop_(hopFor(sampleRate > 0 ? sampleRate : 48000)),
      hann_(kFftSize),
      window_(kFftSize, 0.0f),
      re_(kFftSize, 0.0f),
      im_(kFftSize, 0.0f),
      magnitudes_(kFftSize / 2, 0.0f) {
    for (int i = 0; i < kFftSize; ++i) {
        hann_[static_cast<size_t>(i)] = static_cast<float>(
            0.5 - 0.5 * std::cos(2.0 * kPi * i / (kFftSize - 1)));
    }
    centers_ = BandPlan::centers(bands_);
    edges_ = BandPlan::edges(centers_);
    pending_.reserve(static_cast<size_t>(kFftSize * 4));
}

void SpectrumAnalyzer::reset(int sampleRate) {
    sampleRate_ = sampleRate > 0 ? sampleRate : 48000;
    hop_ = hopFor(sampleRate_);
    primed_ = false;
    pending_.clear();
    std::fill(window_.begin(), window_.end(), 0.0f);
}

void SpectrumAnalyzer::setBandCount(int bands) {
    const int n = BandPlan::clampBands(bands);
    if (n == bands_) {
        return;
    }
    bands_ = n;
    centers_ = BandPlan::centers(bands_);
    edges_ = BandPlan::edges(centers_);
}

bool SpectrumAnalyzer::feed(const float* mono, std::size_t frames,
                            std::vector<float>& levels) {
    if (mono && frames > 0) {
        pending_.insert(pending_.end(), mono, mono + frames);
    }

    // Se a UI ficou parada, não tente analisar um backlog enorme: reinicia a
    // janela com as amostras mais recentes. Isso mantém o medidor em tempo real.
    if (pending_.size() > static_cast<size_t>(kFftSize * 2)) {
        dropBacklog();
    }

    bool produced = false;

    if (!primed_) {
        if (pending_.size() < static_cast<size_t>(kFftSize)) {
            return false;
        }
        const std::size_t start = pending_.size() - kFftSize;
        std::copy(pending_.begin() + static_cast<std::ptrdiff_t>(start),
                  pending_.end(), window_.begin());
        pending_.clear();
        primed_ = true;
        analyze(levels);
        produced = true;
    }

    while (pending_.size() >= static_cast<size_t>(hop_)) {
        std::copy(window_.begin() + hop_, window_.end(), window_.begin());
        std::copy(pending_.begin(),
                  pending_.begin() + static_cast<std::ptrdiff_t>(hop_),
                  window_.end() - hop_);
        pending_.erase(pending_.begin(),
                       pending_.begin() + static_cast<std::ptrdiff_t>(hop_));
        analyze(levels);
        produced = true;
    }
    return produced;
}

void SpectrumAnalyzer::dropBacklog() {
    if (pending_.size() <= static_cast<size_t>(kFftSize)) {
        return;
    }
    const std::size_t start = pending_.size() - static_cast<size_t>(kFftSize);
    pending_.erase(pending_.begin(),
                   pending_.begin() + static_cast<std::ptrdiff_t>(start));
    primed_ = false;
}

void SpectrumAnalyzer::analyze(std::vector<float>& levels) {
    for (int i = 0; i < kFftSize; ++i) {
        const float amostra = window_[static_cast<size_t>(i)];
        re_[static_cast<size_t>(i)] = amostra * hann_[static_cast<size_t>(i)];
        im_[static_cast<size_t>(i)] = 0.0f;
    }

    Fft::transform(re_, im_);

    constexpr int meio = kFftSize / 2;
    for (int k = 1; k < meio; ++k) {
        magnitudes_[static_cast<size_t>(k)] = static_cast<float>(
            4.0 * std::hypot(re_[static_cast<size_t>(k)],
                             im_[static_cast<size_t>(k)]) /
            kFftSize);
    }

    levels.assign(static_cast<size_t>(bands_), 0.0f);
    const double binHz = static_cast<double>(sampleRate_) / kFftSize;

    for (int i = 0; i < bands_; ++i) {
        int inicio = static_cast<int>(
            std::ceil(edges_[static_cast<size_t>(i)] / binHz));
        int fim = static_cast<int>(
            std::floor(edges_[static_cast<size_t>(i + 1)] / binHz));
        inicio = std::max(1, inicio);
        fim = std::min(meio - 1, fim);

        if (inicio > fim) {
            int centro = static_cast<int>(std::lround(
                centers_[static_cast<size_t>(i)] / binHz));
            centro = std::max(1, std::min(meio - 1, centro));
            inicio = centro;
            fim = centro;
        }

        float pico = 0.0f;
        for (int k = inicio; k <= fim; ++k) {
            pico = std::max(pico, magnitudes_[static_cast<size_t>(k)]);
        }

        double db = 20.0 * std::log10(static_cast<double>(pico) + 1e-9);
        db += kTiltDbPerOctave *
              (std::log(centers_[static_cast<size_t>(i)] / 1000.0) /
               std::log(2.0));
        float normalizado = static_cast<float>(
            (db - kMinDb) / (kMaxDb - kMinDb));
        normalizado = std::clamp(normalizado, 0.0f, 1.0f);
        levels[static_cast<size_t>(i)] = normalizado;
    }
}

}  // namespace dsp
