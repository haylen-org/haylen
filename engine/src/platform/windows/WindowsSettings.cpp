#include "platform/windows/WindowsSettings.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace haylen::platform {

// Windows versions without the setting show light apps.
Theme WindowsSettings::readTheme() {
    return readUserNumber(kPersonalizeKey, L"AppsUseLightTheme", 1) == 0 ? Theme::Dark : Theme::Light;
}

// The text size of the accessibility settings is a percentage from 100 to 225, and turning off the animations of Windows clears the client area animations.
AccessibilitySettings WindowsSettings::readAccessibility() {
    HIGHCONTRASTW contrast{.cbSize = static_cast<UINT>(sizeof(HIGHCONTRASTW))};
    const bool highContrast = SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0) != FALSE && (contrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
    BOOL animations = TRUE;
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0);
    BOOL screenReader = FALSE;
    SystemParametersInfoW(SPI_GETSCREENREADER, 0, &screenReader, 0);
    return {
        .textScale = static_cast<float>(readUserNumber(kAccessibilityKey, L"TextScaleFactor", 100)) / 100.0F,
        .reducedMotion = animations == FALSE,
        .highContrast = highContrast,
        .reducedTransparency = readUserNumber(kPersonalizeKey, L"EnableTransparency", 1) == 0,
        .screenReader = screenReader != FALSE,
    };
}

std::uint32_t WindowsSettings::readUserNumber(const wchar_t* key, const wchar_t* name, std::uint32_t missing) {
    DWORD value = 0;
    DWORD size = sizeof(value);
    return RegGetValueW(HKEY_CURRENT_USER, key, name, RRF_RT_REG_DWORD, nullptr, &value, &size) == ERROR_SUCCESS ? value : missing;
}

} // namespace haylen::platform
