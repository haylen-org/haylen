#include "platform/sokol/ReportedState.hpp"

namespace haylen::platform {

bool ReportedState::record(const Event& event) noexcept {
    switch (event.type) {
    case Event::Type::NetworkChanged:
        online = event.online;
        return true;
    case Event::Type::Suspended:
    case Event::Type::Resumed: {
        const bool wasSuspended = isSuspended();
        background = event.type == Event::Type::Suspended;
        return isSuspended() != wasSuspended;
    }
    case Event::Type::FocusLost:
    case Event::Type::FocusGained:
        focused = event.type == Event::Type::FocusGained;
        return true;
    case Event::Type::InterruptionBegan:
    case Event::Type::InterruptionEnded:
        interrupted = event.type == Event::Type::InterruptionBegan;
        return true;
    default:
        return true;
    }
}

std::optional<Event> ReportedState::setPaused(bool value) noexcept {
    const bool wasSuspended = isSuspended();
    paused = value;
    if (isSuspended() == wasSuspended) {
        return std::nullopt;
    }
    return Event{.type = isSuspended() ? Event::Type::Suspended : Event::Type::Resumed};
}

std::vector<Event> ReportedState::getReplay() const {
    std::vector<Event> events;
    if (online) {
        events.push_back({.type = Event::Type::NetworkChanged, .online = *online});
    }
    if (!focused) {
        events.push_back({.type = Event::Type::FocusLost});
    }
    if (interrupted) {
        events.push_back({.type = Event::Type::InterruptionBegan});
    }
    if (isSuspended()) {
        events.push_back({.type = Event::Type::Suspended});
    }
    return events;
}

} // namespace haylen::platform
