// Estado compartilhado: preferências, níveis exibidos e handles das janelas.
#pragma once
#include <windows.h>

#include <vector>

struct AppState {
    int bands = 15;                    // 3..19
    int sensitivity = 50;              // 0..100 -> ganho 0,45..1,55
    int opacity = 85;                  // 0..100 (% do fundo do painel)
    int width = 520;                   // largura do cliente em pixels
    COLORREF color = RGB(0, 229, 255); // cor base dos LEDs
    bool traffic = false;              // tema verde/amarelo/laranja/vermelho
    bool showPeak = true;
    bool topmost = true;
    bool startup = false;
};

extern AppState gState;

// Níveis em 0..1: alvo vindo da FFT, valor suavizado e pico com hold.
extern std::vector<float> gRaw;
extern std::vector<float> gDisplay;
extern std::vector<float> gPeak;
extern std::vector<ULONGLONG> gPeakAt;

extern bool      gCaptureReady;
extern ULONGLONG gLastAudioMs;   // último quadro de FFT recebido
extern ULONGLONG gLastTickMs;    // último tick do timer de áudio

extern HWND gMain;
extern HWND gOpts;

float GainFromSensitivity(int percent);
void  ResetLevels(int bands);
