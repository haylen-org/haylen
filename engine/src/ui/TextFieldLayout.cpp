#include "ui/TextFieldLayout.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <span>

#include "text/Segmenter.hpp"

namespace haylen::ui {

TextFieldLayout::TextFieldLayout(std::shared_ptr<const text::TextLayout> laid, std::u32string codePoints) : layout(std::move(laid)), content(std::move(codePoints)) {}

std::size_t TextFieldLayout::findLine(std::size_t position) const {
    for (std::size_t index = 0; index < layout->lines.size(); ++index) {
        const text::TextLayout::Line& line = layout->lines[index];
        if (line.begin <= position && position <= line.end) {
            return index;
        }
    }
    return layout->lines.size() - 1;
}

// At either end of its line the caret stands at that end of the line in the direction of its paragraph, and elsewhere on the side where the character after it starts, the left of a left-to-right character and the right of a right-to-left one, or at the end of the character before it.
float TextFieldLayout::getCaretX(const text::TextLayout::Line& line, std::size_t position) const {
    const auto characters = std::span(layout->characters).subspan(line.firstCharacter, line.endCharacter - line.firstCharacter);
    if (characters.empty()) {
        return line.box.x;
    }
    if (position <= line.begin || position >= line.end) {
        const bool leftEnd = (position <= line.begin) != line.rightToLeft;
        const auto left = std::ranges::min_element(characters, {}, [](const text::TextLayout::Character& character) { return character.box.x; });
        const auto right = std::ranges::max_element(characters, {}, [](const text::TextLayout::Character& character) { return character.box.getRight(); });
        return leftEnd ? left->box.x : right->box.getRight();
    }
    for (const text::TextLayout::Character& character : characters) {
        if (character.begin <= position && position < character.end) {
            return character.rightToLeft ? character.box.getRight() : character.box.x;
        }
    }
    for (const text::TextLayout::Character& character : characters) {
        if (character.end == position) {
            return character.rightToLeft ? character.box.x : character.box.getRight();
        }
    }
    return line.box.x;
}

math::Rect TextFieldLayout::getCaret(std::size_t position) const {
    const text::TextLayout::Line& line = layout->lines[findLine(position)];
    return {getCaretX(line, position), line.box.y, 0.0F, line.box.height};
}

// A point on the first half of a character puts the caret on that side of it, and a point past either end of the line puts it at that end of the line.
std::size_t TextFieldLayout::hitLine(const text::TextLayout::Line& line, float x) const {
    const auto characters = std::span(layout->characters).subspan(line.firstCharacter, line.endCharacter - line.firstCharacter);
    if (characters.empty()) {
        return line.begin;
    }
    for (const text::TextLayout::Character& character : characters) {
        if (character.box.x <= x && x < character.box.getRight()) {
            const bool leftSide = x < character.box.getCenter().x;
            return leftSide != character.rightToLeft ? character.begin : character.end;
        }
    }
    const auto left = std::ranges::min_element(characters, {}, [](const text::TextLayout::Character& character) { return character.box.x; });
    const bool pastLeft = x < left->box.x;
    return pastLeft != line.rightToLeft ? line.begin : line.end;
}

std::size_t TextFieldLayout::hitTest(math::Vec2 point) const {
    for (const text::TextLayout::Line& line : layout->lines) {
        if (point.y < line.box.getBottom()) {
            return hitLine(line, point.x);
        }
    }
    return hitLine(layout->lines.back(), point.x);
}

// The caret stops at the ends of the line and between its characters, and moves to the nearest stop that shows on the side it goes to, so it never jumps back where the direction of the text changes. Past the end of its line it goes on to the line after it in reading order, or back to the one before.
std::size_t TextFieldLayout::moveAcross(std::size_t position, bool right) const {
    const std::size_t lineIndex = findLine(position);
    const text::TextLayout::Line& line = layout->lines[lineIndex];
    const float current = getCaretX(line, position);
    std::vector<std::size_t> stops{line.begin, line.end};
    for (std::size_t index = line.firstCharacter; index < line.endCharacter; ++index) {
        stops.push_back(layout->characters[index].begin);
        stops.push_back(layout->characters[index].end);
    }
    std::optional<std::pair<float, std::size_t>> best;
    for (const std::size_t stop : stops) {
        const float x = getCaretX(line, stop);
        const bool ahead = right ? x > current + 0.5F : x < current - 0.5F;
        if (ahead && (!best || (right ? x < best->first : x > best->first))) {
            best = std::pair{x, stop};
        }
    }
    if (best) {
        return best->second;
    }

    const bool forward = right != line.rightToLeft;
    if (forward && lineIndex + 1 < layout->lines.size()) {
        return layout->lines[lineIndex + 1].begin;
    }
    if (!forward && lineIndex > 0) {
        return layout->lines[lineIndex - 1].end;
    }
    return position;
}

std::size_t TextFieldLayout::moveAlong(std::size_t position, bool down) const {
    const std::size_t lineIndex = findLine(position);
    if (down ? lineIndex + 1 >= layout->lines.size() : lineIndex == 0) {
        return down ? content.size() : 0;
    }
    return hitLine(layout->lines[down ? lineIndex + 1 : lineIndex - 1], getCaretX(layout->lines[lineIndex], position));
}

std::vector<math::Rect> TextFieldLayout::getSelection(std::size_t begin, std::size_t end) const {
    std::vector<math::Rect> boxes;
    for (const text::TextLayout::Line& line : layout->lines) {
        std::vector<math::Rect> covered;
        for (std::size_t index = line.firstCharacter; index < line.endCharacter; ++index) {
            const text::TextLayout::Character& character = layout->characters[index];
            if (character.begin >= begin && character.end <= end) {
                covered.push_back({character.box.x, line.box.y, character.box.width, line.box.height});
            }
        }
        std::ranges::sort(covered, {}, &math::Rect::x);
        for (const math::Rect& box : covered) {
            if (!boxes.empty() && boxes.back().y == box.y && std::abs(boxes.back().getRight() - box.x) < 0.5F) {
                boxes.back().width = box.getRight() - boxes.back().x;
            } else {
                boxes.push_back(box);
            }
        }
    }
    return boxes;
}

std::pair<std::size_t, std::size_t> TextFieldLayout::getWord(std::size_t position) const {
    std::size_t begin = std::min(position, content.size());
    std::size_t end = begin;
    while (begin > 0 && !text::Segmenter::isSpace(content[begin - 1]) && content[begin - 1] != U'\n') {
        --begin;
    }
    while (end < content.size() && !text::Segmenter::isSpace(content[end]) && content[end] != U'\n') {
        ++end;
    }
    return {begin, end};
}

} // namespace haylen::ui
