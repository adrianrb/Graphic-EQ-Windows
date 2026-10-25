// Faixas logarítmicas e rótulos do graph-eq Android.
#pragma once
#include <string>
#include <vector>

namespace dsp {

class BandPlan {
public:
    static constexpr float kMinHz = 25.0f;
    static constexpr float kMaxHz = 16000.0f;
    static constexpr int kMinBands = 3;
    static constexpr int kMaxBands = 19;

    static int clampBands(int bands);
    static std::vector<float> centers(int bands);
    static std::vector<float> edges(const std::vector<float>& centers);
    static std::wstring label(float hz);
};

}  // namespace dsp
