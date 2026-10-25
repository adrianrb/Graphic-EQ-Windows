#include "app/config.h"

#include <algorithm>
#include <cwchar>
#include <string>

#include "app/ids.h"
#include "app/state.h"

IniWindow gIniWindow;

static std::wstring gIniPath;
static const wchar_t* const SECTION = L"EQ";
static const wchar_t* const RUN_KEY =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t* const RUN_VALUE = L"GraphicEQ";

static void MakeIniPath() {
    wchar_t buf[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring p(buf);
    const size_t pos = p.find_last_of(L'\\');
    gIniPath = (pos == std::wstring::npos ? L"." : p.substr(0, pos)) +
               L"\\GraphicEQ.ini";
}

void ConfigInit() {
    MakeIniPath();
}

void LoadIni() {
    if (gIniPath.empty()) {
        return;
    }

    wchar_t b[64] = {};
    GetPrivateProfileStringW(SECTION, L"X", L"", b, 64, gIniPath.c_str());
    if (*b) {
        gIniWindow.hasXY = true;
        gIniWindow.x = static_cast<int>(wcstol(b, nullptr, 10));
        GetPrivateProfileStringW(SECTION, L"Y", L"0", b, 64, gIniPath.c_str());
        gIniWindow.y = static_cast<int>(wcstol(b, nullptr, 10));
    }

    GetPrivateProfileStringW(SECTION, L"W", L"", b, 64, gIniPath.c_str());
    if (*b) {
        gIniWindow.hasWH = true;
        gIniWindow.w =
            std::clamp(static_cast<int>(wcstol(b, nullptr, 10)),
                       MIN_WIDTH, MAX_WIDTH);
    }

    gState.width = gIniWindow.hasWH ? gIniWindow.w : DEFAULT_WIDTH;
    gState.bands = std::clamp(
        static_cast<int>(GetPrivateProfileIntW(
            SECTION, L"Bands", gState.bands, gIniPath.c_str())),
        3, 19);
    gState.sensitivity = std::clamp(
        static_cast<int>(GetPrivateProfileIntW(
            SECTION, L"Sensitivity", gState.sensitivity, gIniPath.c_str())),
        0, 100);
    gState.opacity = std::clamp(
        static_cast<int>(GetPrivateProfileIntW(
            SECTION, L"Opacity", gState.opacity, gIniPath.c_str())),
        10, 100);
    gState.color = static_cast<COLORREF>(
        GetPrivateProfileIntW(SECTION, L"Color",
                              static_cast<int>(gState.color),
                              gIniPath.c_str()));
    gState.traffic =
        GetPrivateProfileIntW(SECTION, L"Traffic", 0, gIniPath.c_str()) != 0;
    gState.showPeak =
        GetPrivateProfileIntW(SECTION, L"Peak", 1, gIniPath.c_str()) != 0;
    gState.topmost =
        GetPrivateProfileIntW(SECTION, L"Topmost", 1, gIniPath.c_str()) != 0;
    gState.startup =
        GetPrivateProfileIntW(SECTION, L"Startup", 0, gIniPath.c_str()) != 0;
}

void SaveIni() {
    if (gIniPath.empty()) {
        return;
    }

    wchar_t b[64];
    auto wr = [&](const wchar_t* key, int value) {
        swprintf(b, 64, L"%d", value);
        WritePrivateProfileStringW(SECTION, key, b, gIniPath.c_str());
    };

    RECT rc = {};
    if (gMain && GetWindowRect(gMain, &rc)) {
        wr(L"X", rc.left);
        wr(L"Y", rc.top);
        wr(L"W", rc.right - rc.left);
    } else {
        wr(L"W", gState.width);
    }

    wr(L"Bands", gState.bands);
    wr(L"Sensitivity", gState.sensitivity);
    wr(L"Opacity", gState.opacity);
    wr(L"Color", static_cast<int>(gState.color));
    wr(L"Traffic", gState.traffic ? 1 : 0);
    wr(L"Peak", gState.showPeak ? 1 : 0);
    wr(L"Topmost", gState.topmost ? 1 : 0);
    wr(L"Startup", gState.startup ? 1 : 0);
}

void SetStartup(bool on) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, nullptr, 0,
                        KEY_SET_VALUE, nullptr, &key, nullptr) !=
        ERROR_SUCCESS) {
        return;
    }

    if (on) {
        wchar_t exe[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        const std::wstring cmd = L"\"" + std::wstring(exe) + L"\"";
        RegSetValueExW(key, RUN_VALUE, 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(cmd.c_str()),
                       static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(key, RUN_VALUE);
    }
    RegCloseKey(key);
}
