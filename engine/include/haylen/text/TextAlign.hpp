#pragma once

#include <cstdint>

namespace haylen::text {

// Horizontal alignment of each line inside a text block. Start and end follow the direction of the paragraph, so start is the left of left-to-right text and the right of right-to-left text, while left and right keep their side. Fill stretches the spaces of every wrapped line to reach both edges, and the last line of a paragraph stays at the start.
enum class TextAlign : std::uint8_t {
    Start,
    End,
    Left,
    Center,
    Right,
    Fill,
};

} // namespace haylen::text
