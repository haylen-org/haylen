#pragma once

#include <cstdint>

namespace haylen::ui {

// Whether the focus wraps around to the other side of a node when a move would leave it, across its width, its height or both.
enum class FocusWrap : std::uint8_t {
    None,
    Horizontal,
    Vertical,
    Both,
};

} // namespace haylen::ui
