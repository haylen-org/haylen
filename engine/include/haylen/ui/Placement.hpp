#pragma once

#include <cstdint>

namespace haylen::ui {

// Where a GUI lays out its root: inside the safe area, or over the whole visible screen.
enum class Placement : std::uint8_t {
    Safe,
    Screen,
};

} // namespace haylen::ui
