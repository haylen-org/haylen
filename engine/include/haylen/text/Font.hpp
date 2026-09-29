#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "haylen/debug/ObjectCounter.hpp"
#include "haylen/debug/TrackedObject.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/text/TextLayout.hpp"
#include "haylen/text/TextStyle.hpp"

namespace haylen::text {

// A font whose glyph images live in texture pages: a TrueType font drawn through a signed distance field, or a bitmap font drawn from its own images. Sizes are pixel sizes, and every font lays plain text out the same way.
class Font {
  public:
    // One glyph at the native size of the font: the region of its image in a page, where the image goes from the pen on the baseline, and how far the pen moves after it.
    struct Glyph {
        math::Rect source{};
        math::Vec2 offset{};
        float advance = 0.0F;
        std::uint16_t page = 0;
        bool visible = false;
    };

    virtual ~Font() = default;

    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;

    // Lays out UTF-8 text with the top-left of the block at the origin before the anchor is applied.
    [[nodiscard]] TextLayout layout(std::string_view text, const TextStyle& style);
    [[nodiscard]] math::Vec2 measure(std::string_view text, const TextStyle& style);

    // Tells whether the pages hold signed distance fields, which the text shader draws with outlines, weights and soft edges, rather than plain images.
    [[nodiscard]] virtual bool isDistanceField() const noexcept = 0;
    [[nodiscard]] virtual bool hasGlyph(char32_t codePoint) = 0;

    // Returns the glyph of a code point. A TrueType font draws its missing glyph box for a code point it lacks, and a bitmap font draws nothing.
    [[nodiscard]] virtual const Glyph& getGlyph(char32_t codePoint) = 0;

    // Returns the advance adjustment between two glyphs at the native size.
    [[nodiscard]] virtual float getKerning(char32_t left, char32_t right) = 0;
    [[nodiscard]] virtual std::size_t getPageCount() const noexcept = 0;
    [[nodiscard]] virtual const graphics::Texture& getPage(std::size_t index) const = 0;

    // Uploads glyphs added since the last call. The renderer calls it before drawing.
    virtual void sync() = 0;

    // The size the glyph images were made for, the bake size of a TrueType font or the size of a bitmap font, which draws pixel for pixel at it.
    [[nodiscard]] float getNativeSize() const noexcept {
        return metrics.nativeSize;
    }
    [[nodiscard]] float getLineHeight(float size) const noexcept;
    [[nodiscard]] float getAscent(float size) const noexcept;

    // Converts a length in pixels at a text size to the distance field units the text shader reads, where 0.5 spans the spread of the field. Fonts without a distance field convert everything to zero.
    [[nodiscard]] float toDistance(float pixels, float size) const noexcept;

  protected:
    // Line metrics at the native size. The spread is the reach of the distance field in native pixels, zero for a font without one.
    struct Metrics {
        float nativeSize = 0.0F;
        float ascent = 0.0F;
        float lineHeight = 0.0F;
        float spread = 0.0F;
    };

    explicit Font(Metrics fontMetrics) noexcept : metrics(fontMetrics) {}

  private:
    struct Line {
        std::size_t begin = 0;
        std::size_t end = 0;
        float width = 0.0F;
        bool wrapped = false;
    };

    static debug::ObjectCounter counter;

    [[nodiscard]] std::vector<Line> breakLines(const std::u32string& codePoints, const TextStyle& style, float factor);
    [[nodiscard]] float advanceOf(const std::u32string& codePoints, std::size_t index, float factor);

    Metrics metrics;
    debug::TrackedObject tracked{counter};
};

} // namespace haylen::text
