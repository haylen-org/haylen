#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "haylen/debug/ObjectCounter.hpp"
#include "haylen/debug/TrackedObject.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/text/Layout.hpp"
#include "haylen/text/Style.hpp"

namespace haylen::text {

class LayoutCache;

// A font whose glyph images live in texture pages: a TrueType font drawn through a signed distance field, or a bitmap font drawn from its own images. Sizes are pixel sizes, and every font shapes and lays text out the same way.
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

    // A glyph of shaped text at the native size: its index in the font, the code point where its cluster starts, how far the pen moves after it, and where it sits from the pen with y pointing down.
    struct ShapedGlyph {
        std::uint32_t index = 0;
        std::size_t cluster = 0;
        float advance = 0.0F;
        math::Vec2 offset{};
    };

    // A run of text to shape in one script, language and direction. The text around the run gives the letters at its ends their joining forms, and the script is an ISO 15924 tag such as `Arab` or `Deva`.
    struct Run {
        std::u32string_view text;
        std::size_t begin = 0;
        std::size_t end = 0;
        std::uint32_t script = 0;
        std::string_view language;
        bool rightToLeft = false;
    };

    virtual ~Font();

    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;

    // Lays UTF-8 text out with this font alone, with the top-left of the block at the origin and the anchor left to the drawing. Layouts are cached by text and style, so text drawn every frame shapes once.
    [[nodiscard]] std::shared_ptr<const Layout> layout(std::string_view text, const Style& style);
    // Returns the size of the block that drawing the text covers, stretched by the scale of the style.
    [[nodiscard]] math::Vec2 measure(std::string_view text, const Style& style);

    // Tells whether the pages hold signed distance fields, which the text shader draws with outlines, weights and soft edges, rather than plain images.
    [[nodiscard]] virtual bool isDistanceField() const noexcept = 0;
    [[nodiscard]] virtual bool hasGlyph(char32_t codePoint) = 0;

    // Shapes a run into glyphs in visual order, appended to the list. A TrueType font substitutes and places its glyphs through its OpenType tables, which join letters, form ligatures, place marks and kern, while a bitmap font sets one glyph per code point with its kerning pairs, mirrored and reversed in right-to-left runs.
    virtual void shape(const Run& run, std::vector<ShapedGlyph>& shaped) = 0;

    // Returns a glyph by the index shaping gave it. A TrueType font draws its missing glyph box for a code point it lacks, and a bitmap font draws nothing.
    [[nodiscard]] virtual const Glyph& getGlyph(std::uint32_t index) = 0;
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

    explicit Font(Metrics fontMetrics);

  private:
    static debug::ObjectCounter& counter;

    Metrics metrics;
    std::unique_ptr<LayoutCache> layouts;
    debug::TrackedObject tracked{counter};
};

} // namespace haylen::text
