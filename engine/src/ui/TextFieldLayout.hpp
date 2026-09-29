#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/text/TextLayout.hpp"

namespace haylen::ui {

// The text of a field as it shows, shaped and ordered for display, and where its caret goes. Positions are code points of the text, the caret stops between characters, a letter with its marks or the letters a ligature joins, and points are relative to the top-left of the text block. Right-to-left runs place the caret on the side a character starts, and the caret moves on screen, so the right arrow always moves it right.
class TextFieldLayout final {
  public:
    TextFieldLayout(std::shared_ptr<const text::TextLayout> laid, std::u32string codePoints);

    [[nodiscard]] const text::TextLayout& getLayout() const noexcept {
        return *layout;
    }

    // Returns the caret at a position as a line from the top to the bottom of its line.
    [[nodiscard]] math::Rect getCaret(std::size_t position) const;

    // Returns the position whose caret stands nearest to a point.
    [[nodiscard]] std::size_t hitTest(math::Vec2 point) const;

    // Returns the position the caret reaches one character to the left or right on screen, or at the other end of the line before or after when it leaves its line.
    [[nodiscard]] std::size_t moveAcross(std::size_t position, bool right) const;

    // Returns the position on the line above or below whose caret stands nearest to the one of the position.
    [[nodiscard]] std::size_t moveAlong(std::size_t position, bool down) const;

    // Returns the boxes a selection covers on screen, one per run of neighbouring characters, which is several when it crosses lines or directions.
    [[nodiscard]] std::vector<math::Rect> getSelection(std::size_t begin, std::size_t end) const;

    // Returns the word around a position, the run of characters between spaces.
    [[nodiscard]] std::pair<std::size_t, std::size_t> getWord(std::size_t position) const;

  private:
    [[nodiscard]] std::size_t findLine(std::size_t position) const;
    [[nodiscard]] float getCaretX(const text::TextLayout::Line& line, std::size_t position) const;
    [[nodiscard]] std::size_t hitLine(const text::TextLayout::Line& line, float x) const;

    std::shared_ptr<const text::TextLayout> layout;
    std::u32string content;
};

} // namespace haylen::ui
