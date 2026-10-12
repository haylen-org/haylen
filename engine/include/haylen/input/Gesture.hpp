#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <utility>

#include "haylen/math/Vec2.hpp"

namespace haylen::input {

// A recognized gesture in design coordinates. A swipe carries its movement in `delta`, and a pinch carries its center and its scale since exactly two fingers touched the screen.
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

    static constexpr std::array<std::pair<std::string_view, Type>, 5> kTypeNames{{
        {"tap", Type::Tap},
        {"doubleTap", Type::DoubleTap},
        {"longPress", Type::LongPress},
        {"swipe", Type::Swipe},
        {"pinch", Type::Pinch},
    }};

    // Returns `tap`, `doubleTap`, `longPress`, `swipe` or `pinch`.
    [[nodiscard]] static std::string_view typeName(Type value) noexcept;
};

} // namespace haylen::input
