#pragma once

#include <cstdint>

namespace haylen::ui {

// Where a GUI lays out its root: inside the safe area, over the whole visible screen, or inside the area of a node of another GUI, such as the middle between the bars of an app.
enum class Placement : std::uint8_t {
    Safe,
    Screen,
    Region,
};

} // namespace haylen::ui
