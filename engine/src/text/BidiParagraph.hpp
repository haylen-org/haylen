#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "haylen/text/Direction.hpp"

namespace haylen::text {

// One paragraph run through the Unicode Bidirectional Algorithm: the direction it reads in, the embedding level of every code point, where odd levels read right to left, and the visual order of the runs of any line of it.
class BidiParagraph final {
  public:
    struct Run {
        std::size_t begin = 0;
        std::size_t end = 0;
        std::uint8_t level = 0;
    };

    // Analyzes the text, which must stay alive and unchanged while the paragraph is used. A paragraph separator, such as a line feed or U+2029, may only end the text, and one before its end throws std::invalid_argument.
    BidiParagraph(std::u32string_view text, Direction direction);
    ~BidiParagraph();

    [[nodiscard]] bool isRightToLeft() const noexcept;
    [[nodiscard]] std::uint8_t getLevel(std::size_t index) const noexcept;

    // Returns the runs of the line of code points from begin to end, from its left edge to its right edge, after the spaces at its end take the direction of the paragraph.
    [[nodiscard]] std::vector<Run> getVisualRuns(std::size_t begin, std::size_t end) const;

  private:
    struct Analysis;

    std::unique_ptr<Analysis> analysis;
};

} // namespace haylen::text
