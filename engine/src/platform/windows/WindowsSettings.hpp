#pragma once

#include <cstdint>

#include "haylen/platform/AccessibilitySettings.hpp"
#include "haylen/platform/Theme.hpp"

namespace haylen::platform {

// Reads the app theme of the personalization settings and the accessibility settings of the user of Windows, which need no window.
class WindowsSettings final {
  public:
    [[nodiscard]] static Theme readTheme();
    [[nodiscard]] static AccessibilitySettings readAccessibility();

  private:
    static constexpr const wchar_t* kPersonalizeKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
    static constexpr const wchar_t* kAccessibilityKey = L"Software\\Microsoft\\Accessibility";

    [[nodiscard]] static std::uint32_t readUserNumber(const wchar_t* key, const wchar_t* name, std::uint32_t missing);
};

} // namespace haylen::platform
