#pragma once

#include <cstdint>

namespace haylen::text {

// The direction a paragraph reads in. The value `Auto` takes the direction of its first letter with a strong direction, such as an Arabic or a Latin letter, and reads left to right when it has none. Runs of the other direction inside a paragraph, such as English words or numbers in Arabic text, keep their own order.
enum class Direction : std::uint8_t {
    Auto,
    LeftToRight,
    RightToLeft,
};

} // namespace haylen::text
