#include "haylen/platform/System.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "haylen/core/EventBus.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "platform/Host.hpp"

namespace haylen::platform {

const std::array<std::pair<std::string_view, Theme>, 2> System::kThemeNames{{{"light", Theme::Light}, {"dark", Theme::Dark}}};

System::System(Host& owner, core::EventBus& bus, std::string gpuName) : host(owner), events(bus), info(owner.getSystemInfo()), theme(owner.getTheme()), battery(owner.getBattery()) {
    info.gpuName = std::move(gpuName);
}

const SystemInfo& System::getInfo() const noexcept {
    return info;
}

Theme System::getTheme() const noexcept {
    return theme;
}

const Battery& System::getBattery() const noexcept {
    return battery;
}

void System::openUrl(std::string_view url, std::function<void(bool opened)> callback) {
    if (url.empty()) {
        throw std::invalid_argument("Opening a url needs a url.");
    }
    // The platform may answer while it opens the url, and the answer waits for the next pump like any other.
    const std::uint64_t id = nextUrl++;
    // clang-format off
    host.openUrl(url, [shared = std::weak_ptr<Inbox>(inbox), id](bool opened) {
        if (const std::shared_ptr<Inbox> alive = shared.lock()) {
            const std::scoped_lock lock(alive->mutex);
            alive->answers.push_back({.id = id, .opened = opened});
        }
    });
    // clang-format on
    pending.emplace(id, std::move(callback));
}

void System::vibrate(float seconds) {
    if (!std::isfinite(seconds) || seconds <= 0.0F) {
        throw std::invalid_argument("A vibration lasts a positive number of seconds.");
    }
    host.vibrate(seconds);
}

void System::pump() {
    std::vector<Answer> answers;
    {
        const std::scoped_lock lock(inbox->mutex);
        answers.swap(inbox->answers);
    }
    for (const Answer& answer : answers) {
        const auto found = pending.find(answer.id);
        if (found == pending.end()) {
            continue;
        }
        const std::function<void(bool)> callback = std::move(found->second);
        pending.erase(found);
        if (callback) {
            callback(answer.opened);
        }
    }

    const Theme reportedTheme = host.getTheme();
    if (reportedTheme != theme) {
        theme = reportedTheme;
        events.emit(core::LifecycleEvent::kSystemThemeChanged, {{"theme", themeName(theme)}});
    }
    const Battery reportedBattery = host.getBattery();
    if (reportedBattery != battery) {
        battery = reportedBattery;
        events.emit(core::LifecycleEvent::kBatteryChanged, battery.toJson());
    }
}

std::string_view System::themeName(Theme value) noexcept {
    return std::ranges::find(kThemeNames, value, &std::pair<std::string_view, Theme>::second)->first;
}

} // namespace haylen::platform
