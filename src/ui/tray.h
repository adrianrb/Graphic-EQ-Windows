// Ícone na bandeja (Shell_NotifyIcon): clique abre o menu de contexto.
#pragma once
#include <windows.h>

void TrayInit(HICON icon);
void TrayAdd(HWND hwnd);
void TrayRemove();

bool TrayHandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
