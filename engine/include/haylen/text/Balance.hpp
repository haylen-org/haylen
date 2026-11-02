#pragma once

#include <cstdint>

namespace haylen::text {

// How the lines of a wrapped paragraph share its words. The value `None` fills every line as far as it goes, so text that grows as it is typed keeps the lines before its end in place. The value `LastLine` fills the lines the same way, but the last line of a paragraph, or the last line before a line break, takes the last word of the line before it when it would hold one word alone. The value `AllLines` gives every line about the same width, the narrowest that keeps the count of lines, which suits titles, captions and short centered messages.
enum class Balance : std::uint8_t {
    None,
    LastLine,
    AllLines,
};

} // namespace haylen::text
