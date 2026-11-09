#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "haylen/platform/AccessibilitySettings.hpp"
#include "haylen/platform/Battery.hpp"
#include "haylen/platform/Theme.hpp"

namespace haylen::platform {

// The theme, the battery, the power saving, the time zone, the accessibility settings and the languages of the device as the platform last reported them. Platform services report them from any thread whenever the system tells them of a change, and the host reads them on the frame thread once per frame.
class SystemState final {
  public:
    void setTheme(Theme value);
    [[nodiscard]] Theme getTheme() const;
    void setBattery(const Battery& value);
    [[nodiscard]] Battery getBattery() const;
    void setPowerSaving(std::optional<bool> value);
    [[nodiscard]] std::optional<bool> getPowerSaving() const;
    void setTimeZone(std::string value);
    [[nodiscard]] std::string getTimeZone() const;
    void setAccessibility(const AccessibilitySettings& value);
    [[nodiscard]] AccessibilitySettings getAccessibility() const;

    // Takes the languages the person prefers, most preferred first, as the platform writes them, and keeps the canonical form of each once. A text that is no language tag is left out with a warning. The revision grows whenever the list changes.
    void setLanguages(std::span<const std::string> value);
    [[nodiscard]] std::vector<std::string> getLanguages() const;
    [[nodiscard]] std::uint64_t getLanguagesRevision() const;

  private:
    mutable std::mutex mutex;
    Theme theme = Theme::Light;
    Battery battery;
    std::optional<bool> powerSaving;
    std::string timeZone;
    AccessibilitySettings accessibility;
    std::vector<std::string> languages;
    std::uint64_t languagesRevision = 0;
};

} // namespace haylen::platform
