// Preferências em GraphicEQ.ini (ao lado do .exe) e autostart no registro.
#pragma once
#include <windows.h>

struct IniWindow {
    bool hasXY = false;
    bool hasWH = false;
    int x = 0, y = 0;
    int w = 520;
};

extern IniWindow gIniWindow;

void ConfigInit();          // descobre o caminho do GraphicEQ.ini
void LoadIni();             // aplica o .ini no estado global
void SaveIni();             // grava o estado global no .ini
void SetStartup(bool on);   // cria/remove HKCU\...\Run
