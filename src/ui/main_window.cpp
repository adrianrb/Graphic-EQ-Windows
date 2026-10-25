#include "ui/main_window.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "app/config.h"
#include "app/ids.h"
#include "app/state.h"
#include "app/version.h"
#include "audio/capture.h"
#include "dsp/spectrum.h"
#include "gfx/render.h"
#include "ui/menu.h"
#include "ui/options_window.h"
#include "ui/tray.h"

namespace {

dsp::SpectrumAnalyzer gAnalyzer(48000, 15);
std::vector<float> gMono;
std::vector<float> gLevels;

bool gDragging = false;
POINT gDragStart = {};
RECT gDragRect = {};
bool gWasActive = false;
bool gWasAudioStatus = false;
bool gWasCaptureStatus = false;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

void ResetAnalyzerLevels() {
    ResetLevels(gState.bands);
    gLevels.assign(static_cast<size_t>(gState.bands), 0.0f);
    gWasActive = false;
}

void UpdateAudio(ULONGLONG now) {
    gAudio.Poll(gMono);
    gCaptureReady = gAudio.Ready();

    if (gCaptureReady) {
        const int rate = gAudio.SampleRate();
        if (rate > 0 && rate != gAnalyzer.sampleRate()) {
            gAnalyzer.reset(rate);
            gLevels.assign(static_cast<size_t>(gState.bands), 0.0f);
        }
    }

    if (gAnalyzer.bandCount() != gState.bands) {
        gAnalyzer.setBandCount(gState.bands);
    }

    const bool produced =
        gAnalyzer.feed(gMono.data(), gMono.size(), gLevels);
    if (produced && gLevels.size() == gRaw.size()) {
        std::copy(gLevels.begin(), gLevels.end(), gRaw.begin());
        gLastAudioMs = now;
    } else if (gLastAudioMs == 0 || now - gLastAudioMs > 300) {
        std::fill(gRaw.begin(), gRaw.end(), 0.0f);
    }

    if (gLastTickMs == 0) {
        gLastTickMs = now;
    }
    const float dt = static_cast<float>(std::clamp(
        (now - gLastTickMs) / 1000.0, 0.001, 0.1));
    gLastTickMs = now;

    const float attack = 1.0f - std::exp(-dt / 0.035f);
    const float release = 1.0f - std::exp(-dt / 0.100f);
    const float gain = GainFromSensitivity(gState.sensitivity);

    bool active = false;
    for (size_t i = 0; i < gDisplay.size(); ++i) {
        const float target = std::clamp(gRaw[i] * gain, 0.0f, 1.0f);
        float value = gDisplay[i];
        const float factor = target > value ? attack : release;
        value += (target - value) * factor;
        if (value < 0.002f) {
            value = 0.0f;
        }
        gDisplay[i] = value;

        if (value >= gPeak[i]) {
            gPeak[i] = value;
            gPeakAt[i] = now;
        } else if (now - gPeakAt[i] > 350) {
            gPeak[i] = std::max(value, gPeak[i] - 0.55f * dt);
        }
        if (gPeak[i] < 0.00001f) {
            gPeak[i] = 0.0f;
        }

        if (value > 0.004f || gPeak[i] > 0.004f) {
            active = true;
        }
    }

    const bool audioStatus =
        gCaptureReady &&
        (gLastAudioMs != 0 && now - gLastAudioMs <= 1500);
    const bool statusChanged =
        audioStatus != gWasAudioStatus || gCaptureReady != gWasCaptureStatus;
    gWasAudioStatus = audioStatus;
    gWasCaptureStatus = gCaptureReady;

    if (active || gWasActive || statusChanged) {
        RequestRepaint();
    }
    gWasActive = active;
}

void StartDrag(HWND hwnd) {
    gDragging = true;
    GetCursorPos(&gDragStart);
    GetWindowRect(hwnd, &gDragRect);
    SetCapture(hwnd);
}

void UpdateDrag(HWND hwnd) {
    if (!gDragging) {
        return;
    }
    POINT p;
    GetCursorPos(&p);
    SetWindowPos(hwnd, nullptr,
                 gDragRect.left + (p.x - gDragStart.x),
                 gDragRect.top + (p.y - gDragStart.y),
                 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

}  // namespace

bool RegisterMainWindowClass(HINSTANCE hInst, HICON big, HICON small) {
    WNDCLASSEX wc = { sizeof(wc) };
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = MAIN_CLASS;
    wc.hIcon = big;
    wc.hIconSm = small;
    return RegisterClassEx(&wc) != 0;
}

void ApplyTopmost() {
    if (!gMain) {
        return;
    }
    SetWindowPos(gMain, gState.topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
                 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void ApplyWidth(int width) {
    const int w = std::clamp(width, MIN_WIDTH, MAX_WIDTH);
    const int h = std::max(80, static_cast<int>(
                                   std::lround(w * PANEL_ASPECT)));
    gState.width = w;
    if (gMain) {
        SetWindowPos(gMain, nullptr, 0, 0, w, h,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    RequestRepaint();
}

int CurrentScalePct() {
    return static_cast<int>(std::lround(
        gState.width * 100.0 / DEFAULT_WIDTH));
}

void ApplyScale(int pct) {
    ApplyWidth(DEFAULT_WIDTH * std::max(25, pct) / 100);
}

void ApplyBandCount(int bands) {
    const int n = std::clamp(bands, 3, 19);
    if (n == gState.bands && static_cast<int>(gRaw.size()) == n) {
        return;
    }
    gState.bands = n;
    gAnalyzer.setBandCount(n);
    ResetAnalyzerLevels();
    RequestRepaint();
}

void ToggleTopmost() {
    gState.topmost = !gState.topmost;
    ApplyTopmost();
    SaveIni();
    SyncOptions();
}

void ShowAbout() {
    std::wstring msg = L"Graphic EQ \u2014 Sa\u00edda Padr\u00e3o\nVers\u00e3o ";
    msg += APP_VERSION;
    msg += L"\n\nVisualizador de espectro para Windows.\n"
           L"Captura a sa\u00edda padr\u00e3o via WASAPI loopback; "
           L"n\u00e3o altera o \u00e1udio.\n\n"
           L"(2026) Copyleft by adrian.barbosa@gmail.com";
    MessageBoxW(gMain, msg.c_str(), L"Sobre...", MB_OK | MB_ICONINFORMATION);
}

namespace {

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (TrayHandleMessage(hwnd, msg, wParam, lParam)) {
        return 0;
    }

    switch (msg) {
    case WM_CREATE:
        TrayAdd(hwnd);
        ResetAnalyzerLevels();
        gAnalyzer.setBandCount(gState.bands);
        gLastTickMs = GetTickCount64();
        SetTimer(hwnd, IDT_AUDIO, 10, nullptr);
        return 0;

    case WM_TIMER:
        if (wParam == IDT_AUDIO) {
            UpdateAudio(GetTickCount64());
        }
        return 0;

    case WM_SIZE:
        RequestRepaint();
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        RenderLayered();
        return 0;
    }

    case WM_LBUTTONDOWN:
        StartDrag(hwnd);
        return 0;

    case WM_LBUTTONDBLCLK:
        ShowOptions();
        return 0;

    case WM_MOUSEMOVE:
        if (gDragging && (wParam & MK_LBUTTON)) {
            UpdateDrag(hwnd);
        }
        return 0;

    case WM_LBUTTONUP:
    case WM_CAPTURECHANGED:
        if (gDragging) {
            gDragging = false;
            if (msg == WM_LBUTTONUP && GetCapture() == hwnd) {
                ReleaseCapture();
            }
            SaveIni();
        }
        return 0;

    case WM_RBUTTONUP:
        ShowContextMenu(hwnd);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDM_OPTIONS:
            ShowOptions();
            return 0;
        case IDM_TOPMOST:
            ToggleTopmost();
            return 0;
        case IDM_ABOUT:
            ShowAbout();
            return 0;
        case IDM_EXIT:
            DestroyWindow(hwnd);
            return 0;
        default:
            if (LOWORD(wParam) >= IDM_SIZE &&
                LOWORD(wParam) < IDM_SIZE + SCALE_COUNT) {
                ApplyScale(SCALES[LOWORD(wParam) - IDM_SIZE]);
                SaveIni();
                return 0;
            }
            break;
        }
        break;

    case WM_GETMINMAXINFO: {
        RECT rc = { 0, 0, MIN_WIDTH,
                    static_cast<LONG>(std::lround(MIN_WIDTH * PANEL_ASPECT)) };
        AdjustWindowRect(&rc, static_cast<DWORD>(GetWindowLongPtr(hwnd, GWL_STYLE)),
                         FALSE);
        auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
        mmi->ptMinTrackSize.x = rc.right - rc.left;
        mmi->ptMinTrackSize.y = rc.bottom - rc.top;
        return 0;
    }

    case WM_DESTROY:
        SaveIni();
        TrayRemove();
        KillTimer(hwnd, IDT_AUDIO);
        gAudio.Shutdown();
        FreeRender();
        if (gOpts) {
            DestroyWindow(gOpts);
            gOpts = nullptr;
        }
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

}  // namespace
