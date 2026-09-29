#include "haylen/text/Font.hpp"

#include <algorithm>

#include "haylen/core/Utf8.hpp"
#include "text/BreakRules.hpp"

namespace haylen::text {

debug::ObjectCounter Font::counter("Font", debug::ObjectCounter::Kind::Native);

float Font::getLineHeight(float size) const noexcept {
    return metrics.lineHeight * size / metrics.nativeSize;
}

float Font::getAscent(float size) const noexcept {
    return metrics.ascent * size / metrics.nativeSize;
}

float Font::toDistance(float pixels, float size) const noexcept {
    if (metrics.spread <= 0.0F) {
        return 0.0F;
    }
    // The field falls from 1 to 0 across twice the spread at the native size, with the edge at 0.5.
    return pixels * metrics.nativeSize / size / (metrics.spread * 2.0F);
}

float Font::advanceOf(const std::u32string& codePoints, std::size_t index, float factor) {
    float width = getGlyph(codePoints[index]).advance;
    if (index + 1 < codePoints.size()) {
        width += getKerning(codePoints[index], codePoints[index + 1]);
    }
    return width * factor;
}

// Breaks paragraphs into lines, wrapping between the segments that run from one wrap opportunity to the next when a maximum width is set. A line is as wide as its last visible glyph, so trailing spaces never count toward alignment.
std::vector<Font::Line> Font::breakLines(const std::u32string& codePoints, const TextStyle& style, float factor) {
    std::vector<Line> lines;
    std::size_t paragraphBegin = 0;
    while (paragraphBegin <= codePoints.size()) {
        std::size_t paragraphEnd = codePoints.find(U'\n', paragraphBegin);
        if (paragraphEnd == std::u32string::npos) {
            paragraphEnd = codePoints.size();
        }

        Line line{.begin = paragraphBegin, .end = paragraphBegin};
        float penX = 0.0F;
        std::size_t cursor = paragraphBegin;
        while (cursor < paragraphEnd) {
            std::size_t segmentEnd = cursor + 1;
            while (segmentEnd < paragraphEnd && !BreakRules::canBreakBetween(codePoints[segmentEnd - 1], codePoints[segmentEnd])) {
                ++segmentEnd;
            }

            float total = 0.0F;
            float visible = 0.0F;
            for (std::size_t index = cursor; index < segmentEnd; ++index) {
                total += advanceOf(codePoints, index, factor);
                if (!BreakRules::isSpace(codePoints[index])) {
                    visible = total;
                }
            }

            if (style.maxWidth > 0.0F && line.end > line.begin && penX + visible > style.maxWidth) {
                line.wrapped = true;
                lines.push_back(line);
                line = {.begin = cursor, .end = cursor};
                penX = 0.0F;
            }
            line.end = segmentEnd;
            if (visible > 0.0F) {
                line.width = penX + visible;
            }
            penX += total;
            cursor = segmentEnd;
        }
        lines.push_back(line);
        paragraphBegin = paragraphEnd + 1;
    }
    return lines;
}

TextLayout Font::layout(std::string_view text, const TextStyle& style) {
    const std::u32string codePoints = core::Utf8::decode(text);
    const float factor = style.size / metrics.nativeSize;
    const float lineAdvance = getLineHeight(style.size) * style.lineSpacing;
    const std::vector<Line> lines = breakLines(codePoints, style, factor);

    TextLayout result;
    result.lineCount = lines.size();
    float blockWidth = style.maxWidth;
    for (const Line& line : lines) {
        blockWidth = std::max(blockWidth, line.width);
    }
    result.size = {blockWidth, lineAdvance * static_cast<float>(lines.size() - 1) + getLineHeight(style.size)};
    const math::Vec2 anchorOffset = result.size * style.anchor;

    // Emit one quad per visible glyph with its line offset for the alignment, and spread the room left on a wrapped line over its spaces when filling.
    for (std::size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const Line& line = lines[lineIndex];
        float x = 0.0F;
        float spacing = 0.0F;
        if (style.align == TextAlign::Center) {
            x = (blockWidth - line.width) * 0.5F;
        } else if (style.align == TextAlign::Right) {
            x = blockWidth - line.width;
        } else if (style.align == TextAlign::Fill && line.wrapped) {
            std::size_t visibleEnd = line.end;
            while (visibleEnd > line.begin && BreakRules::isSpace(codePoints[visibleEnd - 1])) {
                --visibleEnd;
            }
            const auto spaces = std::count_if(codePoints.begin() + static_cast<std::ptrdiff_t>(line.begin), codePoints.begin() + static_cast<std::ptrdiff_t>(visibleEnd), BreakRules::isSpace);
            spacing = spaces > 0 ? (blockWidth - line.width) / static_cast<float>(spaces) : 0.0F;
        }
        const float baseline = getAscent(style.size) + lineAdvance * static_cast<float>(lineIndex);

        for (std::size_t index = line.begin; index < line.end; ++index) {
            const Glyph& current = getGlyph(codePoints[index]);
            if (current.visible) {
                result.quads.push_back({
                    .position = math::Vec2{x + current.offset.x * factor, baseline + current.offset.y * factor} - anchorOffset,
                    .size = current.source.getSize() * factor,
                    .source = current.source,
                    .page = current.page,
                });
            }
            x += advanceOf(codePoints, index, factor);
            if (BreakRules::isSpace(codePoints[index])) {
                x += spacing;
            }
        }
    }
    return result;
}

math::Vec2 Font::measure(std::string_view text, const TextStyle& style) {
    return layout(text, style).size;
}

} // namespace haylen::text
