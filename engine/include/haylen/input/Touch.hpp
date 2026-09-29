#pragma once

#include <cstdint>

#include "haylen/input/TouchPhase.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::input {

// A finger on the screen in design coordinates. The previous position is where the finger was when the frame started.
struct Touch {
    std::uint64_t id = 0;
    math::Vec2 position{};
    math::Vec2 startPosition{};
    math::Vec2 previousPosition{};
    TouchPhase phase = TouchPhase::Began;
    float duration = 0.0F;

    [[nodiscard]] constexpr bool isActive() const noexcept {
        return phase != TouchPhase::Ended && phase != TouchPhase::Cancelled;
    }
};

} // namespace haylen::input
