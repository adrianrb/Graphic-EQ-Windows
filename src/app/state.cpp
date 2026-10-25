#include "app/state.h"

#include <algorithm>

AppState gState;

std::vector<float> gRaw;
std::vector<float> gDisplay;
std::vector<float> gPeak;
std::vector<ULONGLONG> gPeakAt;

bool      gCaptureReady = false;
ULONGLONG gLastAudioMs = 0;
ULONGLONG gLastTickMs = 0;

HWND gMain = nullptr;
HWND gOpts = nullptr;

float GainFromSensitivity(int percent) {
    const int p = std::clamp(percent, 0, 100);
    return 0.45f + p / 100.0f * 1.1f;
}

void ResetLevels(int bands) {
    const int n = std::max(0, bands);
    gRaw.assign(static_cast<size_t>(n), 0.0f);
    gDisplay.assign(static_cast<size_t>(n), 0.0f);
    gPeak.assign(static_cast<size_t>(n), 0.0f);
    gPeakAt.assign(static_cast<size_t>(n), 0);
}
