#include "ui/tray.h"

#include <shellapi.h>

#include "ui/menu.h"

namespace {
constexpr UINT kTrayIcon = WM_APP + 1;

NOTIFYICONDATAW gNid = {};
UINT gTaskbarCreated = 0;
HICON gTrayIcon = nullptr;
}  // namespace

void TrayInit(HICON icon) {
    gTrayIcon = icon;
    gTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
}

void TrayAdd(HWND hwnd) {
    ZeroMemory(&gNid, sizeof(gNid));
    gNid.cbSize = sizeof(gNid);
    gNid.hWnd = hwnd;
    gNid.uID = 1;
    gNid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    gNid.uCallbackMessage = kTrayIcon;
    gNid.hIcon = gTrayIcon;
    lstrcpynW(gNid.szTip, L"Graphic EQ \u2014 Sa\u00edda Padr\u00e3o",
              static_cast<int>(sizeof(gNid.szTip) / sizeof(gNid.szTip[0])));
    Shell_NotifyIconW(NIM_ADD, &gNid);
}

void TrayRemove() {
    if (gNid.hWnd) {
        Shell_NotifyIconW(NIM_DELETE, &gNid);
    }
    ZeroMemory(&gNid, sizeof(gNid));
}

bool TrayHandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    (void)wParam;
    if (gTaskbarCreated && msg == gTaskbarCreated) {
        TrayAdd(hwnd);
        return true;
    }
    if (msg == kTrayIcon) {
        if (LOWORD(lParam) == WM_LBUTTONUP ||
            LOWORD(lParam) == WM_RBUTTONUP ||
            LOWORD(lParam) == WM_CONTEXTMENU) {
            ShowContextMenu(hwnd, true);
        }
        return true;
    }
    return false;
}
