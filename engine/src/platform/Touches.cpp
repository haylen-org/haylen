#include "platform/Touches.hpp"

#include <algorithm>

namespace haylen::platform {

std::optional<Event> Touches::change(std::uint64_t id, Phase phase, math::Vec2 position, input::KeyModifiers modifiers) {
    Finger* const end = fingers.data() + count;
    Finger* finger = std::find_if(fingers.data(), end, [id](const Finger& candidate) { return candidate.id == id; });
    if (finger == end) {
        if (phase != Phase::Began || count == fingers.size()) {
            return std::nullopt;
        }

        ++count;
    }

    *finger = {.id = id, .position = position};

    Event::Type type = Event::Type::TouchMoved;
    if (phase == Phase::Began) {
        type = Event::Type::TouchBegan;
    } else if (phase == Phase::Ended) {
        type = Event::Type::TouchEnded;
    } else if (phase == Phase::Cancelled) {
        type = Event::Type::TouchCancelled;
    }

    Event event{.type = type, .modifiers = modifiers, .touchCount = count};
    for (std::size_t index = 0; index < count; ++index) {
        event.touches[index] = {.id = fingers[index].id, .position = fingers[index].position, .changed = &fingers[index] == finger};
    }

    if (phase == Phase::Ended || phase == Phase::Cancelled) {
        *finger = fingers[--count];
    }

    return event;
}

} // namespace haylen::platform
