#include "haylen/platform/System.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>

#include "haylen/core/EventBus.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "platform/Host.hpp"

namespace haylen::platform {

const std::array<std::pair<std::string_view, Theme>, 2> System::kThemeNames{{{"light", Theme::Light}, {"dark", Theme::Dark}}};

System::System(Host& owner, Bridge& channel, core::EventBus& bus, std::string gpuName, std::function<std::vector<AppPlugin>()> plugins) : host(owner), bridge(channel), events(bus), appPlugins(std::move(plugins)), info(owner.getSystemInfo()), theme(owner.getTheme()), battery(owner.getBattery()), powerSaving(owner.getPowerSaving()), timeZone(owner.getTimeZone()) {
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

std::optional<bool> System::getPowerSaving() const noexcept {
    return powerSaving;
}

const std::string& System::getTimeZone() const noexcept {
    return timeZone;
}

void System::openUrl(std::string_view url, std::function<void(Permission::Answer answer)> callback) {
    if (url.empty()) {
        throw std::invalid_argument("Opening a url needs a url.");
    }
    host.openUrl(url, awaitAnswer(std::move(callback)));
}

std::string System::getClipboard() const {
    return host.getClipboard();
}

void System::readClipboard(std::function<void(Permission::Answer answer, std::string text)> callback) {
    host.readClipboard(awaitReply(std::move(callback)));
}

void System::setClipboard(std::string_view text, std::function<void(Permission::Answer answer)> callback) {
    host.setClipboard(text, awaitAnswer(std::move(callback)));
}

void System::checkPermission(Permission::Kind kind, std::function<void(Permission::Answer answer)> callback) {
    host.checkPermission(kind, awaitAnswer(std::move(callback)));
}

void System::requestPermission(Permission::Kind kind, std::function<void(Permission::Answer answer)> callback) {
    host.requestPermission(kind, awaitAnswer(std::move(callback)));
}

void System::checkPermission(std::string_view name, std::function<void(Permission::Answer answer)> callback) {
    if (const std::optional<Permission::Kind> kind = Permission::kindFromName(name)) {
        checkPermission(*kind, std::move(callback));
        return;
    }
    askPlugin(name, "permission", std::move(callback));
}

void System::requestPermission(std::string_view name, std::function<void(Permission::Answer answer)> callback) {
    if (const std::optional<Permission::Kind> kind = Permission::kindFromName(name)) {
        requestPermission(*kind, std::move(callback));
        return;
    }
    askPlugin(name, "requestPermission", std::move(callback));
}

// The bridge answers on a later frame, on the frame thread, so the answer of the plugin reaches the callback as the answers of the host do.
void System::askPlugin(std::string_view name, std::string_view method, std::function<void(Permission::Answer answer)> callback) {
    const std::vector<AppPlugin> plugins = appPlugins();
    const auto owner = std::ranges::find_if(plugins, [name](const AppPlugin& plugin) { return std::ranges::find(plugin.permissions, name) != plugin.permissions.end(); });
    if (owner == plugins.end()) {
        throw std::invalid_argument(Permission::unknown(name));
    }

    std::function<void(Permission::Answer)> answer = awaitAnswer(std::move(callback));
    if (!owner->native) {
        answer({.state = Permission::State::Unsupported, .message = std::format("The plugin \"{}\" that owns the permission \"{}\" offers no part on this platform.", owner->id, name)});
        return;
    }

    const Bridge::Payload params{.json = {{"name", std::string(name)}}};
    // clang-format off
    bridge.call(owner->id + "." + std::string(method), params, [plugin = owner->id, permission = std::string(name), answer = std::move(answer)](const Bridge::Result& result) {
        answer(readPluginAnswer(plugin, permission, result));
    });
    // clang-format on
}

Permission::Answer System::readPluginAnswer(const std::string& plugin, const std::string& name, const Bridge::Result& result) {
    if (result.ok) {
        return Permission::Answer::fromJson(result.value.json);
    }
    if (result.error.code == "noHandler") {
        return {.state = Permission::State::Unsupported, .message = std::format("The plugin \"{}\" that owns the permission \"{}\" offers no part on this platform that answers it.", plugin, name)};
    }
    return {.state = Permission::State::Unsupported, .message = std::format("The plugin \"{}\" failed to answer the permission \"{}\". {}", plugin, name, result.error.message)};
}

// The platform answers during the call.
void System::vibrate(float seconds, std::function<void(Permission::Answer answer)> callback) {
    if (!std::isfinite(seconds) || seconds <= 0.0F) {
        throw std::invalid_argument("A vibration lasts a positive number of seconds.");
    }
    awaitAnswer(std::move(callback))(host.vibrate(seconds));
}

// The platform may answer during the call, and the answer waits for the next pump like any other. A call without a callback, such as a copy of the UI, keeps none.
System::Callback System::awaitReply(Callback callback) {
    const std::uint64_t id = nextRequest++;
    if (callback) {
        pending.emplace(id, std::move(callback));
    }
    // clang-format off
    return [shared = std::weak_ptr<Inbox>(inbox), id](Permission::Answer answer, std::string text) {
        if (const std::shared_ptr<Inbox> alive = shared.lock()) {
            const std::scoped_lock lock(alive->mutex);
            alive->replies.push_back({.id = id, .answer = std::move(answer), .text = std::move(text)});
        }
    };
    // clang-format on
}

std::function<void(Permission::Answer)> System::awaitAnswer(std::function<void(Permission::Answer)> callback) {
    // clang-format off
    Callback reply = awaitReply(callback ? Callback([callback = std::move(callback)](Permission::Answer answer, std::string) { callback(std::move(answer)); }) : Callback());
    // clang-format on
    return [reply = std::move(reply)](Permission::Answer answer) { reply(std::move(answer), {}); };
}

void System::guardCrashes(const std::filesystem::path& report) {
    host.guardCrashes(report);
}

void System::setCrashContext(std::string_view json) {
    host.setCrashContext(json);
}

void System::rumbleGamepad(std::size_t index, float low, float high, float seconds) {
    if (index >= input::Input::kMaxGamepads) {
        throw std::invalid_argument("A rumble needs a gamepad from 1 to " + std::to_string(input::Input::kMaxGamepads) + ".");
    }
    if (!(low >= 0.0F && low <= 1.0F) || !(high >= 0.0F && high <= 1.0F)) {
        throw std::invalid_argument("The motors of a rumble run from 0 to 1.");
    }
    if (!std::isfinite(seconds) || seconds <= 0.0F) {
        throw std::invalid_argument("A rumble lasts a positive number of seconds.");
    }
    rumbling.set(index);
    host.rumbleGamepad(index, low, high, seconds);
}

void System::stopRumbles() {
    for (std::size_t index = 0; index < rumbling.size(); ++index) {
        if (rumbling.test(index)) {
            host.rumbleGamepad(index, 0.0F, 0.0F, 0.0F);
        }
    }
    rumbling.reset();
}

void System::pump() {
    std::vector<Reply> replies;
    {
        const std::scoped_lock lock(inbox->mutex);
        replies.swap(inbox->replies);
    }
    for (Reply& reply : replies) {
        const auto found = pending.find(reply.id);
        if (found == pending.end()) {
            continue;
        }
        const Callback callback = std::move(found->second);
        pending.erase(found);
        callback(std::move(reply.answer), std::move(reply.text));
    }
    publishChanges();
}

void System::publishChanges() {
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

    const std::optional<bool> reportedPowerSaving = host.getPowerSaving();
    if (reportedPowerSaving != powerSaving) {
        powerSaving = reportedPowerSaving;
        events.emit(core::LifecycleEvent::kPowerSavingChanged, {{"enabled", powerSaving.has_value() ? core::Json(*powerSaving) : core::Json()}});
    }

    std::string reportedTimeZone = host.getTimeZone();
    if (reportedTimeZone != timeZone) {
        // The C library keeps the zone it read first, so the local times of the app follow a new one only once it reads the zone again.
        timeZone = std::move(reportedTimeZone);
#if defined(_WIN32)
        _tzset();
#else
        tzset();
#endif
        events.emit(core::LifecycleEvent::kTimeZoneChanged, {{"timeZone", timeZone}});
    }
}

std::string_view System::themeName(Theme value) noexcept {
    return std::ranges::find(kThemeNames, value, &std::pair<std::string_view, Theme>::second)->first;
}

} // namespace haylen::platform
