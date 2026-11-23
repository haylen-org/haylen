#pragma once

#include <cstdint>

namespace haylen::text {

// How tall the lines of a text are. The value `Natural` gives each line its own height, the tallest ascent and descent of its fonts at their sizes, grown by the ink of glyphs that reach past those metrics, such as stacked marks and emoji. The value `Uniform` gives every line of the text the tallest ascent and the tallest descent of all its lines, so the baselines follow one rhythm, which tables, code and columns need.
enum class LineHeight : std::uint8_t {
    Natural,
    Uniform,
};

} // namespace haylen::text
