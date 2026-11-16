#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "haylen/input/KeyModifiers.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/platform/Event.hpp"

namespace haylen::platform {

// Follows the fingers on the screen for the platform layers whose system reports each finger on its own, while the engine hands every touch of the screen with each change, with the finger that changed marked. A finger that lifts travels one last time before it leaves the list.
class Touches final {
  public:
    enum class Phase : std::uint8_t {
        Began,
        Moved,
        Ended,
        Cancelled,
    };

    // Returns the event of the change of a finger at a position in framebuffer pixels, or nothing for a finger that is not down or that begins while the engine follows as many fingers as it holds.
    [[nodiscard]] std::optional<Event> change(std::uint64_t id, Phase phase, math::Vec2 position, input::KeyModifiers modifiers);

  private:
    struct Finger {
        std::uint64_t id = 0;
        math::Vec2 position;
    };

    std::array<Finger, Event::kMaxTouchPoints> fingers{};
    std::size_t count = 0;
};

} // namespace haylen::platform
