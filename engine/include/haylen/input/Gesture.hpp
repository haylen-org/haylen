#pragma once

#include <cstdint>
#include <string_view>

#include "haylen/math/Vec2.hpp"

namespace haylen::input {

// A recognized gesture in design coordinates. A swipe carries its movement in delta, and a pinch carries its scale since the second finger landed and its center.
struct Gesture {
    enum class Type : std::uint8_t {
        Tap,
        DoubleTap,
        LongPress,
        Swipe,
        Pinch,
    };

    Type type = Type::Tap;
    math::Vec2 position;
    math::Vec2 delta;
    float scale = 1.0F;

    // Returns "tap", "doubleTap", "longPress", "swipe" or "pinch".
    [[nodiscard]] static std::string_view typeName(Type value) noexcept;
};

} // namespace haylen::input
