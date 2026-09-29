#pragma once

#include <cstdint>

namespace haylen::ui {

// A direction the focus moves in, from the arrow keys, the directional pad, the left stick or a TV remote.
enum class FocusDirection : std::uint8_t {
    Left,
    Right,
    Up,
    Down,
};

} // namespace haylen::ui
