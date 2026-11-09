#include "platform/SystemState.hpp"

#include <utility>

namespace haylen::platform {

void SystemState::setTheme(Theme value) {
    const std::scoped_lock lock(mutex);
    theme = value;
}

Theme SystemState::getTheme() const {
    const std::scoped_lock lock(mutex);
    return theme;
}

void SystemState::setBattery(const Battery& value) {
    const std::scoped_lock lock(mutex);
    battery = value;
}

Battery SystemState::getBattery() const {
    const std::scoped_lock lock(mutex);
    return battery;
}

void SystemState::setPowerSaving(std::optional<bool> value) {
    const std::scoped_lock lock(mutex);
    powerSaving = value;
}

std::optional<bool> SystemState::getPowerSaving() const {
    const std::scoped_lock lock(mutex);
    return powerSaving;
}

void SystemState::setTimeZone(std::string value) {
    const std::scoped_lock lock(mutex);
    timeZone = std::move(value);
}

std::string SystemState::getTimeZone() const {
    const std::scoped_lock lock(mutex);
    return timeZone;
}

void SystemState::setAccessibility(const AccessibilitySettings& value) {
    const std::scoped_lock lock(mutex);
    accessibility = value;
}

AccessibilitySettings SystemState::getAccessibility() const {
    const std::scoped_lock lock(mutex);
    return accessibility;
}

} // namespace haylen::platform
