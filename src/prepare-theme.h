#pragma once
#include <windows.h>

namespace relaxer::launch {
inline bool preparationSystemDark() {
    HIGHCONTRASTW contrast{sizeof(contrast)};
    if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0) &&
        (contrast.dwFlags & HCF_HIGHCONTRASTON)) {
        const auto color = GetSysColor(COLOR_WINDOW);
        return GetRValue(color) + GetGValue(color) + GetBValue(color) < 384;
    }
    DWORD light = 1, size = sizeof(light);
    RegGetValueW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &size);
    return light == 0;
}
struct PreparationColors {
    COLORREF background, text, muted, accent, complete, failed, warning, button, border, track;
    static PreparationColors forScheme(bool dark) {
        return dark ? PreparationColors{RGB(25,25,30), RGB(240,237,241), RGB(160,156,166),
            RGB(103,194,230), RGB(108,201,162), RGB(255,132,123), RGB(235,185,104),
            RGB(40,40,48), RGB(65,65,76), RGB(52,52,62)}
            : PreparationColors{RGB(255,255,255), RGB(35,38,42), RGB(108,116,120),
            RGB(28,113,145), RGB(32,119,91), RGB(184,54,46), RGB(168,104,24),
            RGB(247,246,248), RGB(220,218,224), RGB(233,231,235)};
    }
};
} // namespace relaxer::launch
