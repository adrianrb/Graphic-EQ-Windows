#include "dsp/band_plan.h"

#include <algorithm>
#include <cmath>

namespace dsp {

int BandPlan::clampBands(int bands) {
    return std::max(kMinBands, std::min(kMaxBands, bands));
}

std::vector<float> BandPlan::centers(int bands) {
    const int n = clampBands(bands);
    std::vector<float> result(static_cast<size_t>(n));
    const double razao = std::pow(kMaxHz / kMinHz, 1.0 / (n - 1));
    for (int i = 0; i < n; ++i) {
        result[static_cast<size_t>(i)] =
            static_cast<float>(kMinHz * std::pow(razao, i));
    }
    result.front() = kMinHz;
    result.back() = kMaxHz;
    return result;
}

std::vector<float> BandPlan::edges(const std::vector<float>& centers) {
    const int n = static_cast<int>(centers.size());
    std::vector<float> bordas(static_cast<size_t>(n + 1));
    if (n == 0) {
        return bordas;
    }
    if (n == 1) {
        bordas[0] = centers[0];
        bordas[1] = centers[0];
        return bordas;
    }

    bordas[0] = static_cast<float>(
        std::max(20.0, static_cast<double>(centers[0]) *
                           std::sqrt(centers[0] / centers[1])));
    for (int i = 1; i < n; ++i) {
        bordas[static_cast<size_t>(i)] =
            std::sqrt(centers[static_cast<size_t>(i - 1)] *
                      centers[static_cast<size_t>(i)]);
    }
    bordas[static_cast<size_t>(n)] = static_cast<float>(
        centers[static_cast<size_t>(n - 1)] *
        std::sqrt(centers[static_cast<size_t>(n - 1)] /
                  centers[static_cast<size_t>(n - 2)]));
    return bordas;
}

std::wstring BandPlan::label(float hz) {
    if (hz < 995.0f) {
        return std::to_wstring(static_cast<long long>(std::lround(hz)));
    }

    const float k = hz / 1000.0f;
    if (k >= 9.95f) {
        return std::to_wstring(static_cast<long long>(std::lround(k))) + L"k";
    }

    const float arredondado = std::round(k * 10.0f) / 10.0f;
    const int inteiro = static_cast<int>(arredondado);
    const int decimal =
        static_cast<int>(std::lround((arredondado - inteiro) * 10.0f));
    if (decimal == 0) {
        return std::to_wstring(inteiro) + L"k";
    }
    return std::to_wstring(inteiro) + L"," + std::to_wstring(decimal) + L"k";
}

}  // namespace dsp
