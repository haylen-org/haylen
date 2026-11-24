#pragma once

#include <optional>
#include <span>
#include <string>

#include "haylen/platform/Accessibility.hpp"
#include "haylen/platform/AccessibilitySettings.hpp"
#include "haylen/platform/Battery.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/platform/Theme.hpp"
#include "haylen/platform/ThermalState.hpp"

namespace haylen::port {

// What the system of a port tells the engine whenever it changes, from any thread: the theme, the battery, the power saving mode, the thermal state, the time zone, the languages of the person and the accessibility settings, which belong to the process so every app that starts hears the last report, and the memory warnings, the links that open the app and the actions of the screen reader.
class SystemReports final {
  public:
    static void setTheme(platform::Theme value);
    static void setBattery(const platform::Battery& value);
    static void setPowerSaving(std::optional<bool> value);
    static void setThermalState(platform::ThermalState value);

    // The IANA name of the time zone of the device, such as `America/Sao_Paulo`.
    static void setTimeZone(std::string value);

    // The languages the person prefers, most preferred first, as tags of BCP 47 in any case and with either separator.
    static void setLanguages(std::span<const std::string> value);
    static void setAccessibilitySettings(const platform::AccessibilitySettings& value);

    static void raiseMemoryWarning(platform::Event::MemoryLevel level) noexcept;

    // A link that opened the app or reached it while it runs, which the app hears as `linkOpened`.
    static void receiveLink(std::string url);

    // An action of the screen reader on an element of the tree that `AccessibilityService::update` handed over.
    static void performAccessibility(const platform::Accessibility::Request& request);
};

} // namespace haylen::port
