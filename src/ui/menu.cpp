#include "ui/menu.h"

#include <cstdlib>
#include <cwchar>

#include "app/ids.h"
#include "app/state.h"
#include "ui/main_window.h"

void ShowContextMenu(HWND owner, bool fromTray) {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, IDM_OPTIONS, L"Op\u00e7\u00f5es...");

    HMENU size = CreatePopupMenu();
    const int current = CurrentScalePct();
    int nearIndex = 0;
    for (int i = 0; i < SCALE_COUNT; ++i) {
        const int distance = std::abs(SCALES[i] - current);
        const int best = std::abs(SCALES[nearIndex] - current);
        if (distance < best) {
            nearIndex = i;
        }
    }
    for (int i = 0; i < SCALE_COUNT; ++i) {
        wchar_t label[16];
        swprintf(label, 16, L"%d%%", SCALES[i]);
        AppendMenuW(size, MF_STRING, IDM_SIZE + i, label);
    }
    CheckMenuRadioItem(size, 0, SCALE_COUNT - 1, nearIndex, MF_BYPOSITION);
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(size),
                L"Tamanho:");

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | (gState.topmost ? MF_CHECKED : 0),
                IDM_TOPMOST, L"Sempre vis\u00edvel");
    AppendMenuW(menu, MF_STRING, IDM_ABOUT, L"Sobre...");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDM_EXIT, L"Sair\tAlt+F4");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(owner);
    const UINT flags = TPM_RIGHTBUTTON | TPM_LEFTALIGN |
                       (fromTray ? TPM_BOTTOMALIGN : TPM_TOPALIGN);
    const int x = fromTray ? pt.x - 8 : pt.x;
    const int y = fromTray ? pt.y - 8 : pt.y;
    TrackPopupMenu(menu, flags, x, y, 0, owner, nullptr);
    PostMessageW(owner, WM_NULL, 0, 0);
    DestroyMenu(menu);
}
