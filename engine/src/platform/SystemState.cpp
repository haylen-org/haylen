#include "platform/SystemState.hpp"

#include <algorithm>
#include <optional>
#include <utility>

#include "haylen/core/Log.hpp"
#include "localization/LanguageTag.hpp"

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

void SystemState::setLanguages(std::span<const std::string> value) {
    std::vector<std::string> tags;
    for (const std::string& reported : value) {
        std::optional<std::string> canonical = localization::LanguageTag::parse(reported);
        if (!canonical) {
            core::Log::warning("The system prefers the language \"{}\", which is no language tag, so the engine leaves it out.", reported);
            continue;
        }
        if (std::ranges::find(tags, *canonical) == tags.end()) {
            tags.push_back(std::move(*canonical));
        }
    }

    const std::scoped_lock lock(mutex);
    if (tags == languages) {
        return;
    }
    languages = std::move(tags);
    ++languagesRevision;
}

std::vector<std::string> SystemState::getLanguages() const {
    const std::scoped_lock lock(mutex);
    return languages;
}

std::uint64_t SystemState::getLanguagesRevision() const {
    const std::scoped_lock lock(mutex);
    return languagesRevision;
}

} // namespace haylen::platform
