#include "haylen/platform/Screens.hpp"

#include <format>
#include <stdexcept>

#include "haylen/core/JsonBytes.hpp"
#include "platform/Host.hpp"
#include "platform/ScreenRelay.hpp"
#include "platform/native/NativeApi.hpp"

namespace haylen::platform {

std::atomic<std::uint64_t> Screens::nextId{1};

Screens::Screens(Host& owner, Bridge& channel, std::function<bool()> active) : host(owner), bridge(channel), canOpen(std::move(active)) {}

Screens::~Screens() {
    if (requested) {
        ScreenRelay::forget(requested->id);
    }
}

std::uint64_t Screens::open(std::string plugin, std::string screen, Bridge::Payload params, Options options, Callback callback) {
    if (plugin.empty() || screen.empty()) {
        throw std::invalid_argument("A screen needs the id of its plugin and its name.");
    }
    core::JsonBytes::validate(params.json, params.buffers.size());
    if (options.timeout && options.timeout->count() <= 0) {
        throw std::invalid_argument("The timeout of a screen is a positive duration.");
    }

    // A screen that cannot open fails at the next pump, like every other answer.
    const std::uint64_t id = nextId.fetch_add(1);
    if (ScreenRelay::getShowing()) {
        failed.emplace_back(std::move(callback), Bridge::Result{.error = {.message = std::format("The screen \"{}\" of \"{}\" cannot open while another screen shows.", screen, plugin), .code = "busy"}});
        return id;
    }
    if (!canOpen()) {
        failed.emplace_back(std::move(callback), Bridge::Result{.error = {.message = std::format("The screen \"{}\" of \"{}\" opens only while the app is active.", screen, plugin), .code = "notActive"}});
        return id;
    }

    ScreenRelay::show({.id = id, .plugin = plugin, .name = screen, .state = options.state, .opaque = options.opaque});
    Pending entry{.callback = std::move(callback), .plugin = plugin, .screen = screen};
    if (options.timeout) {
        entry.deadline = std::chrono::steady_clock::now() + *options.timeout;
    }
    pending.emplace(id, std::move(entry));
    requested = ScreenRequest{.id = id, .plugin = std::move(plugin), .screen = std::move(screen), .params = std::move(params), .state = std::move(options.state), .opaque = options.opaque};
    return id;
}

// Native libraries open the screens they registered, and the platform opens every other screen.
void Screens::present() {
    if (!requested) {
        return;
    }
    const ScreenRequest request = std::move(*requested);
    requested.reset();
    if (!NativeApi::openScreen(request)) {
        host.openScreen(request);
    }
}

bool Screens::cancel(std::uint64_t id) {
    const auto found = pending.find(id);
    if (found == pending.end()) {
        return false;
    }
    Pending entry = std::move(found->second);
    pending.erase(found);

    giveUp(id, entry);
    failed.emplace_back(std::move(entry.callback), Bridge::Result{.error = {.message = std::format("The screen \"{}\" of \"{}\" was cancelled.", entry.screen, entry.plugin), .code = "cancelled"}});
    return true;
}

// A screen that the platform never received ends at once, while one that shows keeps the app covered until the platform reports it gone.
void Screens::giveUp(std::uint64_t id, const Pending& entry) {
    if (requested && requested->id == id) {
        requested.reset();
        ScreenRelay::forget(id);
        return;
    }
    ScreenRelay::abandon(id);
    if (!NativeApi::cancelScreen(entry.plugin, entry.screen, id)) {
        host.cancelScreen(id);
    }
}

bool Screens::isShowing() const {
    return ScreenRelay::getShowing().has_value();
}

void Screens::pump() {
    for (ScreenRelay::Ending& ending : ScreenRelay::take()) {
        const auto found = pending.find(ending.screen.id);
        if (found != pending.end()) {
            Callback callback = std::move(found->second.callback);
            pending.erase(found);
            if (callback) {
                callback(std::move(ending.result));
            }
            continue;
        }
        if (ending.screen.abandoned) {
            continue;
        }

        // The call of the screen is gone with the app that opened it, so the end waits for the first listener of the next app, with the state that app gave.
        const ScreenRelay::Screen& screen = ending.screen;
        Bridge::Result& result = ending.result;
        core::Json payload{{"screen", screen.name}, {"state", screen.state}};
        if (result.ok) {
            payload["result"] = std::move(result.value.json);
        } else {
            payload["error"] = {{"message", result.error.message}, {"code", result.error.code}, {"data", result.error.data}};
        }
        bridge.emit(screen.plugin + ".screenRestored", payload.dump(), std::move(result.value.buffers), {.retain = true});
    }

    for (auto& [callback, result] : std::exchange(failed, {})) {
        if (callback) {
            callback(std::move(result));
        }
    }
    expire();
}

std::size_t Screens::getPendingCount() const noexcept {
    return pending.size() + failed.size();
}

void Screens::expire() {
    const auto now = std::chrono::steady_clock::now();
    std::vector<std::uint64_t> overdue;
    for (const auto& [id, entry] : pending) {
        if (entry.deadline && *entry.deadline <= now) {
            overdue.push_back(id);
        }
    }

    for (const std::uint64_t id : overdue) {
        const auto found = pending.find(id);
        if (found == pending.end()) {
            continue;
        }
        Pending entry = std::move(found->second);
        pending.erase(found);
        giveUp(id, entry);
        if (entry.callback) {
            entry.callback({.error = {.message = std::format("The screen \"{}\" of \"{}\" timed out.", entry.screen, entry.plugin), .code = "timeout"}});
        }
    }
}

} // namespace haylen::platform
