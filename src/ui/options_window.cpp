#include "ui/options_window.h"

#include <commctrl.h>
#include <cwchar>

#include <algorithm>

#include "app/config.h"
#include "app/ids.h"
#include "app/state.h"
#include "gfx/render.h"
#include "ui/main_window.h"

namespace {

HINSTANCE gInst = nullptr;
HBRUSH gSwatchBrush = nullptr;
COLORREF gCustomColors[16] = {};

LRESULT CALLBACK OptionsProc(HWND, UINT, WPARAM, LPARAM);

void UpdateSwatchBrush() {
    if (gSwatchBrush) {
        DeleteObject(gSwatchBrush);
    }
    gSwatchBrush = CreateSolidBrush(gState.color);
}

void SetLabel(HWND h, int id, const wchar_t* fmt, int value) {
    wchar_t buf[64];
    swprintf(buf, 64, fmt, value);
    SetWindowTextW(GetDlgItem(h, id), buf);
}

void PickColor(HWND owner) {
    CHOOSECOLORW cc = {};
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = owner;
    cc.rgbResult = gState.color;
    cc.lpCustColors = gCustomColors;
    cc.Flags = CC_RGBINIT | CC_FULLOPEN | CC_ANYCOLOR;
    if (!ChooseColorW(&cc)) {
        return;
    }
    gState.color = cc.rgbResult;
    UpdateSwatchBrush();
    InvalidatePalette();
    RequestRepaint();
    SaveIni();
}

void CreateControls(HWND h) {
    const HINSTANCE inst =
        reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(h, GWLP_HINSTANCE));
    HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

    auto mk = [&](const wchar_t* cls, const wchar_t* text, DWORD style,
                  int x, int y, int w, int hh, int id) -> HWND {
        HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style,
                                 x, y, w, hh, h,
                                 reinterpret_cast<HMENU>(
                                     static_cast<INT_PTR>(id)),
                                 inst, nullptr);
        if (c) {
            SendMessageW(c, WM_SETFONT,
                         reinterpret_cast<WPARAM>(font), TRUE);
        }
        return c;
    };

    mk(L"STATIC", L"An\u00e1lise", SS_LEFT, 12, 10, 200, 18, 0);

    mk(L"STATIC", L"Faixas (3\u201319): 15", SS_LEFT,
       12, 32, 300, 18, IDC_LBL_BANDS);
    HWND bands = mk(TRACKBAR_CLASSW, L"",
                    TBS_HORZ | TBS_AUTOTICKS, 12, 52, 488, 32,
                    IDC_SLIDER_BANDS);
    SendMessageW(bands, TBM_SETRANGE, TRUE, MAKELPARAM(3, 19));
    SendMessageW(bands, TBM_SETTICFREQ, 1, 0);

    mk(L"STATIC", L"Sensibilidade: 50%", SS_LEFT,
       12, 90, 300, 18, IDC_LBL_SENS);
    HWND sens = mk(TRACKBAR_CLASSW, L"",
                   TBS_HORZ | TBS_AUTOTICKS, 12, 110, 488, 32,
                   IDC_SLIDER_SENS);
    SendMessageW(sens, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
    SendMessageW(sens, TBM_SETTICFREQ, 10, 0);

    mk(L"STATIC", L"Apar\u00eancia", SS_LEFT, 12, 150, 200, 18, 0);

    mk(L"STATIC", L"Largura do painel: 520 px", SS_LEFT,
       12, 172, 340, 18, IDC_LBL_WIDTH);
    HWND width = mk(TRACKBAR_CLASSW, L"",
                    TBS_HORZ | TBS_AUTOTICKS, 12, 192, 488, 32,
                    IDC_SLIDER_WIDTH);
    SendMessageW(width, TBM_SETRANGE, TRUE, MAKELPARAM(MIN_WIDTH, MAX_WIDTH));
    SendMessageW(width, TBM_SETTICFREQ, 100, 0);

    mk(L"STATIC", L"Opacidade do painel: 85%", SS_LEFT,
       12, 230, 340, 18, IDC_LBL_OPACITY);
    HWND opacity = mk(TRACKBAR_CLASSW, L"",
                      TBS_HORZ | TBS_AUTOTICKS, 12, 250, 488, 32,
                      IDC_SLIDER_OPACITY);
    SendMessageW(opacity, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
    SendMessageW(opacity, TBM_SETTICFREQ, 10, 0);

    mk(L"BUTTON", L"Tema sem\u00e1foro (verde/amarelo/laranja/vermelho)",
       BS_AUTOCHECKBOX, 12, 292, 420, 20, IDC_CHK_TRAFFIC);
    mk(L"BUTTON", L"Escolher cor dos LEDs...", BS_PUSHBUTTON,
       12, 320, 200, 28, IDC_BTN_COLOR);
    mk(L"STATIC", L"", WS_BORDER | SS_NOTIFY,
       224, 320, 56, 28, IDC_SWATCH);

    mk(L"BUTTON", L"Exibir pico (hold de 350 ms)", BS_AUTOCHECKBOX,
       12, 354, 300, 20, IDC_CHK_PEAK);
    mk(L"BUTTON", L"Sempre vis\u00edvel", BS_AUTOCHECKBOX,
       12, 382, 220, 20, IDC_CHK_TOPMOST);
    mk(L"BUTTON", L"Iniciar com o Windows", BS_AUTOCHECKBOX,
       250, 382, 240, 20, IDC_CHK_STARTUP);

    mk(L"BUTTON", L"Salvar", BS_PUSHBUTTON,
       12, 418, 90, 28, IDC_BTN_SAVE);
    mk(L"BUTTON", L"Fechar", BS_PUSHBUTTON,
       112, 418, 90, 28, IDC_BTN_CLOSE);
    mk(L"STATIC",
       L"Arraste o painel com o bot\u00e3o esquerdo.\n"
       L"Clique com o direito para o menu.",
       SS_LEFT, 230, 414, 258, 34, 0);

    SyncOptions();
}

}  // namespace

void OptionsInit() {
    UpdateSwatchBrush();
}

void OptionsCleanup() {
    if (gSwatchBrush) {
        DeleteObject(gSwatchBrush);
        gSwatchBrush = nullptr;
    }
}

bool RegisterOptionsClass(HINSTANCE hInst, HICON big, HICON small) {
    gInst = hInst;
    WNDCLASSEX wc = { sizeof(wc) };
    wc.lpfnWndProc = OptionsProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = OPT_CLASS;
    wc.hIcon = big;
    wc.hIconSm = small;
    return RegisterClassEx(&wc) != 0;
}

void SyncOptions() {
    if (!gOpts) {
        return;
    }

    SetLabel(gOpts, IDC_LBL_BANDS, L"Faixas (3\u201319): %d", gState.bands);
    SetLabel(gOpts, IDC_LBL_SENS, L"Sensibilidade: %d%%",
             gState.sensitivity);
    SetLabel(gOpts, IDC_LBL_WIDTH, L"Largura do painel: %d px",
             gState.width);
    SetLabel(gOpts, IDC_LBL_OPACITY, L"Opacidade do painel: %d%%",
             gState.opacity);

    SendMessageW(GetDlgItem(gOpts, IDC_SLIDER_BANDS), TBM_SETPOS, TRUE,
                 gState.bands);
    SendMessageW(GetDlgItem(gOpts, IDC_SLIDER_SENS), TBM_SETPOS, TRUE,
                 gState.sensitivity);
    SendMessageW(GetDlgItem(gOpts, IDC_SLIDER_WIDTH), TBM_SETPOS, TRUE,
                 gState.width);
    SendMessageW(GetDlgItem(gOpts, IDC_SLIDER_OPACITY), TBM_SETPOS, TRUE,
                 gState.opacity);

    CheckDlgButton(gOpts, IDC_CHK_TRAFFIC,
                   gState.traffic ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(gOpts, IDC_CHK_TOPMOST,
                   gState.topmost ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(gOpts, IDC_CHK_STARTUP,
                   gState.startup ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(gOpts, IDC_CHK_PEAK,
                   gState.showPeak ? BST_CHECKED : BST_UNCHECKED);
    EnableWindow(GetDlgItem(gOpts, IDC_BTN_COLOR), !gState.traffic);
}

void ShowOptions() {
    if (!gOpts) {
        RECT rc = { 0, 0, OPT_CW, OPT_CH };
        AdjustWindowRect(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE);
        gOpts = CreateWindowExW(
            0, OPT_CLASS, L"Op\u00e7\u00f5es...",
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
            CW_USEDEFAULT, CW_USEDEFAULT,
            rc.right - rc.left, rc.bottom - rc.top,
            nullptr, nullptr, gInst, nullptr);
        if (!gOpts) {
            return;
        }
    }

    SyncOptions();

    RECT vr = {};
    RECT orc = {};
    GetWindowRect(gMain, &vr);
    GetWindowRect(gOpts, &orc);
    const int ow = orc.right - orc.left;
    const int oh = orc.bottom - orc.top;
    int x = vr.left + ((vr.right - vr.left) - ow) / 2;
    int y = vr.bottom + 4;

    HMONITOR mon = MonitorFromRect(&vr, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfoW(mon, &mi);
    if (x + ow > mi.rcWork.right) {
        x = mi.rcWork.right - ow - 4;
    }
    if (y + oh > mi.rcWork.bottom) {
        y = vr.top - oh - 4;
    }
    x = std::max(x, static_cast<int>(mi.rcWork.left) + 4);
    y = std::max(y, static_cast<int>(mi.rcWork.top) + 4);

    SetWindowPos(gOpts, HWND_TOP, x, y, 0, 0,
                 SWP_NOSIZE | SWP_SHOWWINDOW);
    ShowWindow(gOpts, SW_SHOW);
    SetForegroundWindow(gOpts);
}

namespace {

LRESULT CALLBACK OptionsProc(HWND h, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        CreateControls(h);
        return 0;

    case WM_COMMAND:
        if (HIWORD(wParam) == BN_CLICKED) {
            switch (LOWORD(wParam)) {
            case IDC_CHK_TRAFFIC:
                gState.traffic =
                    IsDlgButtonChecked(h, IDC_CHK_TRAFFIC) == BST_CHECKED;
                EnableWindow(GetDlgItem(h, IDC_BTN_COLOR), !gState.traffic);
                InvalidatePalette();
                RequestRepaint();
                SaveIni();
                break;
            case IDC_CHK_TOPMOST:
                gState.topmost =
                    IsDlgButtonChecked(h, IDC_CHK_TOPMOST) == BST_CHECKED;
                ApplyTopmost();
                SaveIni();
                break;
            case IDC_CHK_STARTUP:
                gState.startup =
                    IsDlgButtonChecked(h, IDC_CHK_STARTUP) == BST_CHECKED;
                SetStartup(gState.startup);
                SaveIni();
                break;
            case IDC_CHK_PEAK:
                gState.showPeak =
                    IsDlgButtonChecked(h, IDC_CHK_PEAK) == BST_CHECKED;
                RequestRepaint();
                SaveIni();
                break;
            case IDC_BTN_COLOR:
                PickColor(h);
                break;
            case IDC_BTN_SAVE:
                SaveIni();
                SetWindowTextW(GetDlgItem(h, IDC_BTN_SAVE), L"Salvo!");
                SetTimer(h, IDC_SAVE_FEEDBACK, 1200, nullptr);
                break;
            case IDC_BTN_CLOSE:
                ShowWindow(h, SW_HIDE);
                break;
            default:
                break;
            }
        }
        return 0;

    case WM_HSCROLL: {
        HWND slider = reinterpret_cast<HWND>(lParam);
        if (!slider) {
            break;
        }
        const int id = GetDlgCtrlID(slider);
        const int pos = static_cast<int>(
            SendMessageW(slider, TBM_GETPOS, 0, 0));

        switch (id) {
        case IDC_SLIDER_BANDS:
            ApplyBandCount(pos);
            SyncOptions();
            break;
        case IDC_SLIDER_SENS:
            gState.sensitivity = std::clamp(pos, 0, 100);
            SetLabel(h, IDC_LBL_SENS, L"Sensibilidade: %d%%",
                     gState.sensitivity);
            RequestRepaint();
            break;
        case IDC_SLIDER_WIDTH:
            ApplyWidth(pos);
            SetLabel(h, IDC_LBL_WIDTH, L"Largura do painel: %d px",
                     gState.width);
            break;
        case IDC_SLIDER_OPACITY:
            gState.opacity = std::clamp(pos, 0, 100);
            SetLabel(h, IDC_LBL_OPACITY, L"Opacidade do painel: %d%%",
                     gState.opacity);
            RequestRepaint();
            break;
        default:
            break;
        }

        if (LOWORD(wParam) == TB_ENDTRACK) {
            SaveIni();
        }
        return 0;
    }

    case WM_TIMER:
        if (wParam == IDC_SAVE_FEEDBACK) {
            SetWindowTextW(GetDlgItem(h, IDC_BTN_SAVE), L"Salvar");
            KillTimer(h, IDC_SAVE_FEEDBACK);
        }
        return 0;

    case WM_CTLCOLORSTATIC:
        if (reinterpret_cast<HWND>(lParam) ==
                GetDlgItem(h, IDC_SWATCH) &&
            gSwatchBrush) {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            SetBkColor(hdc, gState.color);
            SetTextColor(hdc, gState.color);
            return reinterpret_cast<LRESULT>(gSwatchBrush);
        }
        break;

    case WM_CLOSE:
        ShowWindow(h, SW_HIDE);
        return 0;
    }

    return DefWindowProc(h, msg, wParam, lParam);
}

}  // namespace
