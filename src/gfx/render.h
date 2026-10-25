// Desenho do painel (GDI) e entrega via UpdateLayeredWindow.
#pragma once
#include <windows.h>

void DrawPanel(HDC hdc, int width, int height);
void RenderLayered();
void RequestRepaint();
void InvalidatePalette();
void FreeRender();
