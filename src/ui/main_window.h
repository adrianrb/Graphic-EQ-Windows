// Janela principal do analisador: timer de áudio, pintura, arrasto e menu.
#pragma once
#include <windows.h>

bool RegisterMainWindowClass(HINSTANCE hInst, HICON big, HICON small);

void ApplyTopmost();
void ApplyWidth(int width);     // redimensiona mantendo o canto superior esquerdo
int  CurrentScalePct();
void ApplyScale(int pct);
void ApplyBandCount(int bands);
void ToggleTopmost();
void ShowAbout();
