#include "haylen/port/SystemReports.hpp"

#include <utility>

#include "platform/AccessibilityRelay.hpp"
#include "platform/LinkRelay.hpp"
#include "platform/MemoryWarning.hpp"
#include "platform/NativeHost.hpp"

namespace haylen::port {

void SystemReports::setTheme(platform::Theme value) {
    platform::NativeHost::getSystemState().setTheme(value);
}

void SystemReports::setBattery(const platform::Battery& value) {
    platform::NativeHost::getSystemState().setBattery(value);
}

void SystemReports::setPowerSaving(std::optional<bool> value) {
    platform::NativeHost::getSystemState().setPowerSaving(value);
}

void SystemReports::setTimeZone(std::string value) {
    platform::NativeHost::getSystemState().setTimeZone(std::move(value));
}

void SystemReports::setLanguages(std::span<const std::string> value) {
    platform::NativeHost::getSystemState().setLanguages(value);
}

void SystemReports::setAccessibilitySettings(const platform::AccessibilitySettings& value) {
    platform::NativeHost::getSystemState().setAccessibility(value);
}

void SystemReports::raiseMemoryWarning(platform::Event::MemoryLevel level) noexcept {
    platform::MemoryWarning::raise(level);
}

void SystemReports::receiveLink(std::string url) {
    platform::LinkRelay::receive(std::move(url));
}

void SystemReports::performAccessibility(const platform::Accessibility::Request& request) {
    platform::AccessibilityRelay::perform(request);
}

} // namespace haylen::port
