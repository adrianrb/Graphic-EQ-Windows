// Constantes, IDs e nomes de classe compartilhados pelos módulos.
#pragma once
#include <windows.h>

// Layout / análise
inline constexpr int DEFAULT_WIDTH = 520;
inline constexpr float PANEL_ASPECT = 0.46f;
inline constexpr int MIN_WIDTH = 260;
inline constexpr int MAX_WIDTH = 1300;

// Escalas do menu Tamanho (largura base = DEFAULT_WIDTH).
inline constexpr int SCALES[] = { 75, 100, 125, 150, 200, 250 };
inline constexpr int SCALE_COUNT =
    static_cast<int>(sizeof(SCALES) / sizeof(SCALES[0]));

// IDs de comando/controle
enum {
    IDM_OPTIONS = 100,
    IDM_ABOUT,
    IDM_EXIT,
    IDM_TOPMOST,
    IDM_SIZE = 110,            // ..IDM_SIZE+SCALE_COUNT-1

    IDC_SLIDER_BANDS = 200,
    IDC_SLIDER_SENS,
    IDC_SLIDER_WIDTH,
    IDC_SLIDER_OPACITY,
    IDC_CHK_TRAFFIC,
    IDC_CHK_TOPMOST,
    IDC_CHK_STARTUP,
    IDC_CHK_PEAK,
    IDC_BTN_COLOR,
    IDC_SWATCH,
    IDC_BTN_SAVE,
    IDC_BTN_CLOSE,
    IDC_LBL_BANDS,
    IDC_LBL_SENS,
    IDC_LBL_WIDTH,
    IDC_LBL_OPACITY,

    IDC_SAVE_FEEDBACK = 300,
    IDT_AUDIO = 1,
};

// Janelas
inline const wchar_t* const MAIN_CLASS = L"GraphicEQ";
inline const wchar_t* const OPT_CLASS  = L"GraphicEQOptions";
inline constexpr int OPT_CW = 520, OPT_CH = 460;
