#pragma once

#include <cstdint>

namespace haylen::text {

// Horizontal alignment of each line inside a text block. Fill stretches the spaces of every wrapped line to reach both edges, and the last line of a paragraph stays at the left.
enum class TextAlign : std::uint8_t {
    Left,
    Center,
    Right,
    Fill,
};

} // namespace haylen::text
