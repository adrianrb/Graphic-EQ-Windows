// Janela "Opções...": bandas, sensibilidade, largura, opacidade, cor/tema,
// topmost e autostart.
#pragma once
#include <windows.h>

bool RegisterOptionsClass(HINSTANCE hInst, HICON big, HICON small);
void OptionsInit();
void OptionsCleanup();
void ShowOptions();
void SyncOptions();
