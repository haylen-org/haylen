#include "haylen/platform/Displays.hpp"

#include <algorithm>
#include <utility>

#include "haylen/core/EventBus.hpp"
#include "haylen/core/LifecycleEvent.hpp"

namespace haylen::platform {

Displays::Displays(Window& source, core::EventBus& bus) : window(source), events(bus), displays(source.getDisplays()), windowDisplay(source.getDisplayId()) {}

std::span<const Display> Displays::getAll() const noexcept {
    return displays;
}

const Display* Displays::find(std::string_view id) const noexcept {
    const auto found = std::ranges::find(displays, id, &Display::id);
    return found != displays.end() ? &*found : nullptr;
}

const Display& Displays::getPrimary() const noexcept {
    const auto primary = std::ranges::find_if(displays, &Display::primary);
    return primary != displays.end() ? *primary : displays.front();
}

const Display& Displays::getWindowDisplay() const noexcept {
    const Display* found = find(windowDisplay);
    return found != nullptr ? *found : getPrimary();
}

void Displays::refresh() {
    const std::vector<Display> previous = std::exchange(displays, window.getDisplays());
    for (const Display& display : previous) {
        if (find(display.id) == nullptr) {
            events.emit(core::LifecycleEvent::kDisplayDisconnected, {{"display", display.id}, {"name", display.name}});
        }
    }

    for (const Display& display : displays) {
        const auto known = std::ranges::find(previous, display.id, &Display::id);
        if (known == previous.end()) {
            events.emit(core::LifecycleEvent::kDisplayConnected, {{"display", display.id}, {"name", display.name}});
        } else if (*known != display) {
            events.emit(core::LifecycleEvent::kDisplayChanged, {{"display", display.id}, {"name", display.name}});
        }
    }
    refreshWindowDisplay();
}

void Displays::refreshWindowDisplay() {
    std::string display = window.getDisplayId();
    if (display != windowDisplay) {
        windowDisplay = std::move(display);
        events.emit(core::LifecycleEvent::kWindowDisplayChanged, {{"display", windowDisplay}});
    }
}

// The platform may answer during the request, and the answer waits for the next pump like any other.
void Displays::request(std::function<void(Window::DisplayAccess access)> callback) {
    const std::uint64_t id = nextRequest++;
    pending.emplace(id, std::move(callback));
    // clang-format off
    window.requestDisplays([shared = std::weak_ptr<Inbox>(inbox), id](Window::DisplayAccess access) {
        if (const std::shared_ptr<Inbox> alive = shared.lock()) {
            const std::scoped_lock lock(alive->mutex);
            alive->answers.push_back({.id = id, .access = access});
        }
    });
    // clang-format on
}

void Displays::pump() {
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
        const std::function<void(Window::DisplayAccess)> callback = std::move(found->second);
        pending.erase(found);
        callback(answer.access);
    }
}

} // namespace haylen::platform
