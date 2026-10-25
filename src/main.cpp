// Graphic EQ — visualizador de espectro da saída padrão do Windows.
// Win32 + GDI + WASAPI loopback, sem dependências externas.
//
// Interação:
//  - Arraste com o botão esquerdo para mover o painel.
//  - Duplo clique ou clique direito → Opções...
//  - Menu de tamanho, sempre visível, Sobre e Sair (Alt+F4).
//  - Ícone na bandeja com o mesmo menu.
//  - Configuração em GraphicEQ.ini, ao lado do executável.

#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <commctrl.h>

#include <algorithm>
#include <cmath>

#include "app/config.h"
#include "app/ids.h"
#include "app/state.h"
#include "audio/capture.h"
#include "res/resource.h"
#include "ui/main_window.h"
#include "ui/options_window.h"
#include "ui/tray.h"

namespace {

void EnsureWindowVisible(HWND hwnd) {
    RECT wr = {};
    GetWindowRect(hwnd, &wr);
    HMONITOR mon = MonitorFromRect(&wr, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };
    if (!GetMonitorInfoW(mon, &mi)) {
        return;
    }

    RECT inter = {};
    if (IntersectRect(&inter, &wr, &mi.rcWork) &&
        (inter.right - inter.left) >= 40 && (inter.bottom - inter.top) >= 32) {
        return;
    }

    const int w = wr.right - wr.left;
    const int h = wr.bottom - wr.top;
    const int x = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - w) / 2;
    const int y = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - h) / 2;
    SetWindowPos(hwnd, nullptr, x, y, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow) {
    HANDLE single = CreateMutexW(nullptr, FALSE,
                                 L"Local\\GraphicEQ_SingleInstance");
    if (single && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND previous = FindWindowW(MAIN_CLASS, nullptr);
        if (previous) {
            if (IsIconic(previous)) {
                ShowWindow(previous, SW_RESTORE);
            }
            SetForegroundWindow(previous);
        }
        CloseHandle(single);
        return 0;
    }

    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);

    ConfigInit();
    LoadIni();
    if (gState.startup) {
        SetStartup(true);   // conserta o caminho se o .exe foi movido
    }
    OptionsInit();

    HICON bigIcon = static_cast<HICON>(LoadImageW(
        hInst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
        GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0));
    HICON smallIcon = static_cast<HICON>(LoadImageW(
        hInst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));

    TrayInit(smallIcon);
    RegisterMainWindowClass(hInst, bigIcon, smallIcon);
    RegisterOptionsClass(hInst, bigIcon, smallIcon);

    const int width = std::clamp(gState.width, MIN_WIDTH, MAX_WIDTH);
    const int height = std::max(80, static_cast<int>(
                                      std::lround(width * PANEL_ASPECT)));
    gState.width = width;

    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    if (gIniWindow.hasXY) {
        x = gIniWindow.x;
        y = gIniWindow.y;
    } else {
        RECT work = {};
        if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0)) {
            x = work.right - width - 24;
            y = work.top + 24;
        }
    }

    gMain = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW, MAIN_CLASS,
        L"Graphic EQ \u2014 Sa\u00edda Padr\u00e3o", WS_POPUP,
        x, y, width, height, nullptr, nullptr, hInst, nullptr);
    if (!gMain) {
        OptionsCleanup();
        if (smallIcon) DestroyIcon(smallIcon);
        if (bigIcon) DestroyIcon(bigIcon);
        CoUninitialize();
        return 1;
    }

    ApplyTopmost();
    ShowWindow(gMain, nCmdShow);
    UpdateWindow(gMain);
    EnsureWindowVisible(gMain);

    // Se falhar (sem dispositivo), o Poll tenta de novo a cada ~1 s.
    gAudio.Init();

    ACCEL accel[1] = { { static_cast<BYTE>(FVIRTKEY | FALT), VK_F4,
                         IDM_EXIT } };
    HACCEL hAccel = CreateAcceleratorTableW(accel, 1);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (hAccel && TranslateAcceleratorW(gMain, hAccel, &msg)) {
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (hAccel) DestroyAcceleratorTable(hAccel);
    OptionsCleanup();
    if (smallIcon) DestroyIcon(smallIcon);
    if (bigIcon) DestroyIcon(bigIcon);
    CoUninitialize();
    if (single) CloseHandle(single);
    return static_cast<int>(msg.wParam);
}
