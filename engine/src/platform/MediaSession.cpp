#include "haylen/platform/MediaSession.hpp"

#include <algorithm>
#include <stdexcept>

#include "haylen/core/EventBus.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "platform/Host.hpp"

namespace haylen::platform {

std::mutex& MediaSession::mutex = *new std::mutex();
MediaSession* MediaSession::active = nullptr;

MediaSession::MediaSession(Host& owner, core::EventBus& bus) : host(owner), events(bus) {
    const std::scoped_lock lock(mutex);
    active = this;
}

MediaSession::~MediaSession() {
    if (nowPlaying) {
        host.setNowPlaying(std::nullopt);
    }
    const std::scoped_lock lock(mutex);
    if (active == this) {
        active = nullptr;
    }
}

void MediaSession::setNowPlaying(NowPlaying value) {
    if (!(value.duration >= 0.0) || !(value.position >= 0.0)) {
        throw std::invalid_argument("The duration and the position of what plays cannot be negative.");
    }
    nowPlaying = std::move(value);
    host.setNowPlaying(nowPlaying);
}

void MediaSession::clear() {
    if (nowPlaying) {
        nowPlaying.reset();
        host.setNowPlaying(std::nullopt);
    }
}

void MediaSession::receive(Command command, double position) {
    const std::scoped_lock lock(mutex);
    if (active != nullptr) {
        active->received.push_back({.command = command, .position = position});
    }
}

void MediaSession::pump() {
    {
        const std::scoped_lock lock(mutex);
        if (received.empty()) {
            return;
        }
        delivering.swap(received);
    }
    for (const Received& entry : delivering) {
        core::Json payload{{"command", commandName(entry.command)}};
        if (entry.command == Command::Seek) {
            payload["position"] = entry.position;
        }
        events.emit(core::LifecycleEvent::kMediaCommand, payload);
    }
    delivering.clear();
}

std::optional<MediaSession::Command> MediaSession::commandFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kCommandNames, name, &std::pair<std::string_view, Command>::first);
    return found != kCommandNames.end() ? std::optional<Command>(found->second) : std::nullopt;
}

std::string_view MediaSession::commandName(Command value) noexcept {
    return std::ranges::find(kCommandNames, value, &std::pair<std::string_view, Command>::second)->first;
}

} // namespace haylen::platform
