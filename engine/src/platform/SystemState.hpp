#pragma once

#include <mutex>
#include <optional>
#include <string>

#include "haylen/platform/AccessibilitySettings.hpp"
#include "haylen/platform/Battery.hpp"
#include "haylen/platform/Theme.hpp"

namespace haylen::platform {

// The theme, the battery, the power saving, the time zone and the accessibility settings of the device as the platform last reported them. Platform services report them from any thread whenever the system tells them of a change, and the host reads them on the frame thread once per frame.
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

  private:
    mutable std::mutex mutex;
    Theme theme = Theme::Light;
    Battery battery;
    std::optional<bool> powerSaving;
    std::string timeZone;
    AccessibilitySettings accessibility;
};

} // namespace haylen::platform
