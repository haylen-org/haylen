#include "platform/ReturnHold.hpp"

#include <algorithm>
#include <cstddef>

#include "haylen/platform/Event.hpp"

namespace haylen::platform {

// The keys and fingers that a window stops hearing while it is away never end, so they stop counting.
void ReturnHold::handleEvent(const Event& event, float pointScale) {
    switch (event.type) {
    case Event::Type::FocusLost:
    case Event::Type::Suspended:
        escapeHeld = false;
        pauseHeld = false;
        fingers.clear();
        return;
    case Event::Type::KeyDown:
    case Event::Type::KeyUp:
        if (event.key == input::Key::Escape) {
            escapeHeld = event.type == Event::Type::KeyDown;
        } else if (event.key == input::Key::Pause) {
            pauseHeld = event.type == Event::Type::KeyDown;
        }
        return;
    case Event::Type::TouchBegan:
    case Event::Type::TouchMoved:
    case Event::Type::TouchEnded:
    case Event::Type::TouchCancelled:
        break;
    default:
        return;
    }

    for (std::size_t index = 0; index < event.touchCount; ++index) {
        const TouchPoint& point = event.touches[index];
        if (!point.changed) {
            continue;
        }
        const auto found = std::ranges::find(fingers, point.id, &Finger::id);
        if (event.type == Event::Type::TouchBegan) {
            fingers.push_back({.id = point.id, .start = point.position});
        } else if (found == fingers.end()) {
            continue;
        } else if (event.type == Event::Type::TouchMoved) {
            found->still = found->still && (point.position - found->start).getLength() <= kFingerTravel * pointScale;
        } else {
            fingers.erase(found);
        }
    }
}

// A hold completes once, and the next one starts only after every key, button and finger of it let go.
bool ReturnHold::update(float seconds, bool backHeld) {
    if (!isHolding(backHeld)) {
        held = 0.0F;
        completed = false;
        return false;
    }
    if (completed) {
        return false;
    }
    held += seconds;
    completed = held >= kSeconds;
    return completed;
}

float ReturnHold::getProgress() const noexcept {
    if (completed || held < kHintDelay) {
        return 0.0F;
    }
    return std::clamp((held - kHintDelay) / (kSeconds - kHintDelay), 0.0F, 1.0F);
}

bool ReturnHold::isHolding(bool backHeld) const noexcept {
    const bool fingersHeld = static_cast<int>(fingers.size()) == kFingers && std::ranges::all_of(fingers, &Finger::still);
    return escapeHeld || pauseHeld || backHeld || fingersHeld;
}

} // namespace haylen::platform
