#include "gfx/render.h"

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <string>
#include <vector>

#include "app/ids.h"
#include "app/state.h"
#include "dsp/band_plan.h"

namespace {

constexpr COLORREF kPanelColor = RGB(6, 20, 25);
constexpr COLORREF kLedOff = RGB(12, 43, 49);

// O DIB 32bpp guarda 0x00RRGGBB; COLORREF guarda 0x00BBGGRR. A comparação do
// fundo em RenderLayered precisa usar a ordem do DIB.
constexpr DWORD RgbDword(COLORREF color) {
    return (static_cast<DWORD>(GetRValue(color)) << 16) |
           (static_cast<DWORD>(GetGValue(color)) << 8) |
           static_cast<DWORD>(GetBValue(color));
}
constexpr DWORD kPanelRgb = RgbDword(kPanelColor);
static_assert(kPanelRgb == 0x00061419u,
              "kPanelRgb deve usar a ordem 0x00RRGGBB do DIB");
constexpr COLORREF kTextHeader = RGB(155, 246, 255);
constexpr COLORREF kTextLabel = RGB(127, 230, 242);
constexpr COLORREF kTextStatus = RGB(47, 169, 184);
constexpr COLORREF kTextIdle = RGB(63, 124, 134);

struct Palette {
    int segments = -1;
    COLORREF base = 0;
    bool traffic = false;
    std::vector<HBRUSH> on;
    std::vector<HBRUSH> off;
    HBRUSH peak = nullptr;
};

Palette gPalette;
HFONT gHeaderFont = nullptr;
HFONT gLabelFont = nullptr;
int gFontWidth = 0;

HBITMAP gDib = nullptr;
HDC gDibDC = nullptr;
void* gDibBits = nullptr;
int gDibW = 0;
int gDibH = 0;

BYTE Byte(float value) {
    return static_cast<BYTE>(std::clamp(value, 0.0f, 255.0f));
}

COLORREF ScaleColor(COLORREF color, float factor) {
    return RGB(Byte(GetRValue(color) * factor),
               Byte(GetGValue(color) * factor),
               Byte(GetBValue(color) * factor));
}

void RgbToHsv(COLORREF color, float& h, float& s, float& v) {
    const float r = GetRValue(color) / 255.0f;
    const float g = GetGValue(color) / 255.0f;
    const float b = GetBValue(color) / 255.0f;
    const float maxc = std::max({ r, g, b });
    const float minc = std::min({ r, g, b });
    const float delta = maxc - minc;

    v = maxc;
    s = maxc <= 0.0f ? 0.0f : delta / maxc;
    if (delta <= 0.0f) {
        h = 0.0f;
    } else if (maxc == r) {
        h = 60.0f * std::fmod((g - b) / delta, 6.0f);
    } else if (maxc == g) {
        h = 60.0f * (((b - r) / delta) + 2.0f);
    } else {
        h = 60.0f * (((r - g) / delta) + 4.0f);
    }
    if (h < 0.0f) {
        h += 360.0f;
    }
}

COLORREF HsvToRgb(float h, float s, float v) {
    h = std::fmod(h, 360.0f);
    if (h < 0.0f) {
        h += 360.0f;
    }
    s = std::clamp(s, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);

    const float c = v * s;
    const float x = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    const float m = v - c;
    float r = 0.0f, g = 0.0f, b = 0.0f;
    if (h < 60.0f)       { r = c; g = x; b = 0; }
    else if (h < 120.0f) { r = x; g = c; b = 0; }
    else if (h < 180.0f) { r = 0; g = c; b = x; }
    else if (h < 240.0f) { r = 0; g = x; b = c; }
    else if (h < 300.0f) { r = x; g = 0; b = c; }
    else                 { r = c; g = 0; b = x; }

    return RGB(Byte((r + m) * 255.0f), Byte((g + m) * 255.0f),
               Byte((b + m) * 255.0f));
}

COLORREF LedColor(float t) {
    if (gState.traffic) {
        if (t < 0.60f) return RGB(43, 224, 122);
        if (t < 0.85f) return RGB(255, 224, 77);
        if (t < 0.90f) return RGB(255, 167, 38);
        return RGB(255, 77, 77);
    }

    float h = 0.0f, s = 0.0f, v = 0.0f;
    RgbToHsv(gState.color, h, s, v);
    const float valor = 0.72f + 0.28f * t;
    const float saturacao = s * (1.0f - 0.5f * t * t);
    return HsvToRgb(h, saturacao, valor);
}

COLORREF PeakColor() {
    if (gState.traffic) {
        return RGB(255, 255, 255);
    }
    float h = 0.0f, s = 0.0f, v = 0.0f;
    RgbToHsv(gState.color, h, s, v);
    return HsvToRgb(h, s * 0.15f, 1.0f);
}

void DeletePalette() {
    for (HBRUSH b : gPalette.on) {
        if (b) DeleteObject(b);
    }
    for (HBRUSH b : gPalette.off) {
        if (b) DeleteObject(b);
    }
    if (gPalette.peak) {
        DeleteObject(gPalette.peak);
    }
    gPalette = Palette{};
}

void EnsurePalette(int segments) {
    if (gPalette.segments == segments &&
        gPalette.base == gState.color &&
        gPalette.traffic == gState.traffic &&
        static_cast<int>(gPalette.on.size()) == segments) {
        return;
    }

    DeletePalette();
    gPalette.segments = segments;
    gPalette.base = gState.color;
    gPalette.traffic = gState.traffic;
    gPalette.on.resize(static_cast<size_t>(segments));
    gPalette.off.resize(static_cast<size_t>(segments));

    for (int i = 0; i < segments; ++i) {
        const float t = segments <= 1
                            ? 0.0f
                            : static_cast<float>(i) / (segments - 1);
        const COLORREF on = LedColor(t);
        const COLORREF off = gState.traffic ? ScaleColor(on, 0.30f) : kLedOff;
        gPalette.on[static_cast<size_t>(i)] = CreateSolidBrush(on);
        gPalette.off[static_cast<size_t>(i)] = CreateSolidBrush(off);
    }
    gPalette.peak = CreateSolidBrush(PeakColor());
}

void EnsureFonts(int width) {
    if (gFontWidth == width && gHeaderFont && gLabelFont) {
        return;
    }

    if (gHeaderFont) DeleteObject(gHeaderFont);
    if (gLabelFont) DeleteObject(gLabelFont);

    const float scale = std::clamp(width / static_cast<float>(DEFAULT_WIDTH),
                                   0.5f, 2.0f);
    const int headerPx = std::max(9, static_cast<int>(std::lround(13 * scale)));
    const int labelPx = std::max(7, static_cast<int>(std::lround(9.5f * scale)));

    gHeaderFont = CreateFontW(
        -headerPx, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FIXED_PITCH | FF_MODERN, L"Consolas");
    gLabelFont = CreateFontW(
        -labelPx, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FIXED_PITCH | FF_MODERN, L"Consolas");
    gFontWidth = width;
}

void FreeDib() {
    if (gDibDC) {
        DeleteDC(gDibDC);
        gDibDC = nullptr;
    }
    if (gDib) {
        DeleteObject(gDib);
        gDib = nullptr;
    }
    gDibBits = nullptr;
    gDibW = 0;
    gDibH = 0;
}

bool EnsureDib(int width, int height) {
    if (gDib && gDibW == width && gDibH == height) {
        return true;
    }

    FreeDib();
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = width;
    bi.bmiHeader.biHeight = -height;   // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    HDC screen = GetDC(nullptr);
    gDib = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &gDibBits, nullptr, 0);
    gDibDC = CreateCompatibleDC(screen);
    ReleaseDC(nullptr, screen);
    if (!gDib || !gDibDC || !gDibBits) {
        FreeDib();
        return false;
    }
    SelectObject(gDibDC, gDib);
    gDibW = width;
    gDibH = height;
    return true;
}

}  // namespace

void DrawPanel(HDC hdc, int width, int height) {
    if (!hdc || width <= 0 || height <= 0) {
        return;
    }

    RECT all = { 0, 0, width, height };
    HBRUSH bg = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(hdc, &all, bg);
    DeleteObject(bg);

    const float scale = std::clamp(width / static_cast<float>(DEFAULT_WIDTH),
                                   0.5f, 2.2f);
    const int pad = std::max(6, static_cast<int>(std::lround(8 * scale)));
    const int radius = std::max(6, static_cast<int>(std::lround(13 * scale)));

    HBRUSH panel = CreateSolidBrush(kPanelColor);
    HGDIOBJ oldBrush = SelectObject(hdc, panel);
    HGDIOBJ oldPen = SelectObject(hdc, GetStockObject(NULL_PEN));
    RoundRect(hdc, 0, 0, width, height, radius * 2, radius * 2);
    SelectObject(hdc, oldBrush);
    DeleteObject(panel);

    const COLORREF borderColor =
        gState.traffic ? RGB(43, 224, 122) : gState.color;
    HPEN border = CreatePen(PS_SOLID, std::max(1, static_cast<int>(
                                               std::lround(scale))),
                            borderColor);
    SelectObject(hdc, border);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc, 0, 0, width - 1, height - 1, radius * 2, radius * 2);
    SelectObject(hdc, oldPen);
    DeleteObject(border);

    EnsureFonts(width);
    SetBkMode(hdc, TRANSPARENT);

    HGDIOBJ oldFont = SelectObject(hdc, gHeaderFont);
    SetTextColor(hdc, kTextHeader);
    const std::wstring header =
        L"EQUALIZADOR \u00b7 " + std::to_wstring(gState.bands) + L" FAIXAS";
    TextOutW(hdc, pad + static_cast<int>(std::lround(2 * scale)), pad,
             header.c_str(), static_cast<int>(header.size()));

    const ULONGLONG now = GetTickCount64();
    const wchar_t* status = L"\u00c1UDIO ATIVO";
    COLORREF statusColor = kTextStatus;
    if (!gCaptureReady) {
        status = L"SEM CAPTURA";
        statusColor = kTextIdle;
    } else if (gLastAudioMs == 0 || now - gLastAudioMs > 1500) {
        status = L"SEM \u00c1UDIO";
        statusColor = kTextIdle;
    }
    SetTextColor(hdc, statusColor);
    SIZE statusSize = {};
    GetTextExtentPoint32W(hdc, status, static_cast<int>(wcslen(status)),
                          &statusSize);
    TextOutW(hdc, width - pad - statusSize.cx, pad, status,
             static_cast<int>(wcslen(status)));

    const int headerPx = std::max(9, static_cast<int>(std::lround(13 * scale)));
    const int dividerY = pad + headerPx + static_cast<int>(std::lround(4 * scale));
    HPEN divider = CreatePen(PS_SOLID, 1, borderColor);
    SelectObject(hdc, divider);
    MoveToEx(hdc, pad, dividerY, nullptr);
    LineTo(hdc, width - pad, dividerY);
    SelectObject(hdc, oldPen);
    DeleteObject(divider);

    const int labelH = std::max(12, static_cast<int>(std::lround(14 * scale)));
    const int top = dividerY + std::max(2, static_cast<int>(std::lround(4 * scale)));
    const int base = height - pad - labelH;
    const int right = width - pad;
    const int bands = gState.bands;
    if (base <= top || right <= pad || bands <= 0) {
        SelectObject(hdc, oldFont);
        return;
    }

    const float gapScale = std::max(0.6f, std::min(1.0f, 15.0f / bands));
    const int gapX = std::max(1, static_cast<int>(std::lround(
                                     2.0f * scale * gapScale)));
    const int colW = (right - pad - gapX * (bands - 1)) / bands;
    if (colW < 2) {
        SelectObject(hdc, oldFont);
        return;
    }

    const int ledH = std::max(3, static_cast<int>(std::lround(5 * scale)));
    const int ledGap = std::max(1, static_cast<int>(std::lround(2 * scale)));
    int segments = (base - top + ledGap) / (ledH + ledGap);
    segments = std::max(6, segments);
    while (segments > 6 &&
           segments * ledH + (segments - 1) * ledGap > base - top) {
        --segments;
    }
    EnsurePalette(segments);

    // Os LEDs são preenchidos apenas com o brush; sem isso a caneta padrão do
    // DC desenha um contorno preto que vira alpha 1 e deixa o desktop aparecer
    // mesmo com a opacidade do painel em 100%.
    SelectObject(hdc, GetStockObject(NULL_PEN));

    SelectObject(hdc, gLabelFont);
    SetTextColor(hdc, kTextLabel);

    const std::vector<float> centers =
        dsp::BandPlan::centers(bands);
    const bool allLabels = colW >= static_cast<int>(std::lround(24.0f * scale));

    for (int i = 0; i < bands; ++i) {
        const int x = pad + i * (colW + gapX);
        const float raw = i < static_cast<int>(gDisplay.size())
                              ? gDisplay[static_cast<size_t>(i)]
                              : 0.0f;
        int lit = static_cast<int>(std::lround(raw * segments));
        if (raw > 0.02f && lit < 1) {
            lit = 1;
        }

        for (int s = 0; s < segments; ++s) {
            const int y = base - (s + 1) * ledH - s * ledGap;
            HBRUSH brush = s < lit ? gPalette.on[static_cast<size_t>(s)]
                                   : gPalette.off[static_cast<size_t>(s)];
            SelectObject(hdc, brush);
            const int r = std::max(1, ledH / 3);
            RoundRect(hdc, x, y, x + colW, y + ledH, r, r);
        }

        if (gState.showPeak && i < static_cast<int>(gPeak.size()) &&
            gPeak[static_cast<size_t>(i)] > 0.01f) {
            int pos = static_cast<int>(
                          std::lround(gPeak[static_cast<size_t>(i)] * segments)) - 1;
            pos = std::clamp(pos, 0, segments - 1);
            const int y = base - (pos + 1) * ledH - pos * ledGap;
            SelectObject(hdc, gPalette.peak);
            const int r = std::max(1, ledH / 3);
            RoundRect(hdc, x, y, x + colW, y + ledH, r, r);
        }

        if (allLabels || i == 0 || i == bands - 1 || i % 2 == 0) {
            const std::wstring label =
                dsp::BandPlan::label(centers[static_cast<size_t>(i)]);
            RECT tr = { x, height - pad - labelH, x + colW, height - pad };
            DrawTextW(hdc, label.c_str(), static_cast<int>(label.size()), &tr,
                      DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        }
    }

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldFont);
}

void RenderLayered() {
    if (!gMain) {
        return;
    }

    RECT rc = {};
    GetClientRect(gMain, &rc);
    const int width = rc.right - rc.left;
    const int height = rc.bottom - rc.top;
    if (width <= 0 || height <= 0) {
        return;
    }
    if (!EnsureDib(width, height)) {
        return;
    }

    DrawPanel(gDibDC, width, height);
    GdiFlush();

    // O GDI desenha sem alpha. O fundo do painel é convertido para a opacidade
    // escolhida (0 = totalmente transparente, 100 = totalmente opaco); LEDs,
    // texto e borda ficam sempre opacos.
    const BYTE panelAlpha = static_cast<BYTE>(std::clamp(
        255 * gState.opacity / 100, 0, 255));
    auto* pixels = static_cast<DWORD*>(gDibBits);
    const int total = width * height;
    for (int i = 0; i < total; ++i) {
        const DWORD rgb = pixels[i] & 0x00FFFFFFu;
        BYTE alpha = 255;
        if (rgb == kPanelRgb) {
            alpha = panelAlpha;
        } else if (rgb == 0) {
            // Cantos arredondados: em 0% somem por completo; acima disso ficam
            // com alpha 1 para manter a área arrastável.
            alpha = panelAlpha == 0 ? 0 : 1;
        }
        const BYTE r = static_cast<BYTE>((pixels[i] >> 16) & 0xFF);
        const BYTE g = static_cast<BYTE>((pixels[i] >> 8) & 0xFF);
        const BYTE b = static_cast<BYTE>(pixels[i] & 0xFF);
        const BYTE pr = static_cast<BYTE>(r * alpha / 255);
        const BYTE pg = static_cast<BYTE>(g * alpha / 255);
        const BYTE pb = static_cast<BYTE>(b * alpha / 255);
        pixels[i] = (static_cast<DWORD>(alpha) << 24) |
                    (static_cast<DWORD>(pr) << 16) |
                    (static_cast<DWORD>(pg) << 8) |
                    static_cast<DWORD>(pb);
    }

    RECT wr = {};
    GetWindowRect(gMain, &wr);
    HDC screen = GetDC(nullptr);
    POINT src = { 0, 0 };
    POINT dst = { wr.left, wr.top };
    SIZE size = { width, height };
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UpdateLayeredWindow(gMain, screen, &dst, &size, gDibDC, &src, 0, &blend,
                        ULW_ALPHA);
    ReleaseDC(nullptr, screen);
}

void RequestRepaint() {
    if (gMain) {
        InvalidateRect(gMain, nullptr, FALSE);
    }
}

void InvalidatePalette() {
    DeletePalette();
}

void FreeRender() {
    DeletePalette();
    if (gHeaderFont) {
        DeleteObject(gHeaderFont);
        gHeaderFont = nullptr;
    }
    if (gLabelFont) {
        DeleteObject(gLabelFont);
        gLabelFont = nullptr;
    }
    gFontWidth = 0;
    FreeDib();
}
