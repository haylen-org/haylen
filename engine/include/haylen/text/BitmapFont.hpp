#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/text/Font.hpp"

namespace haylen::text {

// A font drawn from prepared images: a BMFont made by tools such as BMFont, Hiero or Littera, or a grid of equal cells in one image. It draws pixel for pixel at its native size and scales at other sizes, and code points it lacks draw nothing. It has no OpenType tables, so it sets one glyph per code point with its kerning pairs and cannot join or reorder the letters of complex scripts.
class BitmapFont final : public Font {
  public:
    // One character of the font: its region in a page, where that region goes from the pen on the baseline, and how far the pen moves after it.
    struct Character {
        char32_t codePoint = 0;
        math::Rect source{};
        math::Vec2 offset{};
        float advance = 0.0F;
        std::uint16_t page = 0;
    };

    struct Kerning {
        char32_t left = 0;
        char32_t right = 0;
        float amount = 0.0F;
    };

    // What a BMFont file says: the size it was made at, the height of a line and of the baseline below the top of a line, the image of each page relative to the file, its characters and its kerning pairs.
    struct Description {
        float size = 0.0F;
        float lineHeight = 0.0F;
        float base = 0.0F;
        std::vector<std::string> pages;
        std::vector<Character> characters;
        std::vector<Kerning> kernings;
    };

    // A grid font: the characters of the cells in reading order as UTF-8, the size of a cell, the gap between cells and around the grid, the advance of every character, the height of a line and the baseline below the top of a cell. A zero advance, line height or baseline takes the size of the cell.
    struct Grid {
        std::string characters;
        float cellWidth = 0.0F;
        float cellHeight = 0.0F;
        math::Vec2 spacing{};
        math::Vec2 margin{};
        float advance = 0.0F;
        float lineHeight = 0.0F;
        float baseline = 0.0F;
    };

    BitmapFont(const Description& description, std::vector<graphics::Texture> pageTextures);

    // Reads a BMFont file in its text or binary format.
    [[nodiscard]] static Description parse(std::span<const std::uint8_t> bytes);
    [[nodiscard]] static Description describeGrid(const Grid& grid, math::Vec2 imageSize);

    [[nodiscard]] bool isDistanceField() const noexcept override {
        return false;
    }
    [[nodiscard]] bool hasGlyph(char32_t codePoint) override;
    void shape(const Run& run, std::vector<ShapedGlyph>& shaped) override;

    // The index of a glyph of a bitmap font is its code point.
    [[nodiscard]] const Glyph& getGlyph(std::uint32_t index) override;
    [[nodiscard]] std::size_t getPageCount() const noexcept override {
        return pages.size();
    }
    [[nodiscard]] const graphics::Texture& getPage(std::size_t index) const override;
    void sync() override {}

  private:
    static constexpr std::string_view kBinaryMagic = "BMF";

    [[nodiscard]] static Metrics readMetrics(const Description& description);
    [[nodiscard]] static Description parseText(std::string_view text);
    [[nodiscard]] static Description parseBinary(std::span<const std::uint8_t> bytes);
    [[nodiscard]] static std::uint64_t pairKey(char32_t left, char32_t right) noexcept;

    [[nodiscard]] float getKerning(char32_t left, char32_t right) const;

    std::vector<graphics::Texture> pages;
    std::unordered_map<std::uint32_t, Glyph> glyphs;
    std::unordered_map<std::uint64_t, float> kernings;
    Glyph missing;
};

} // namespace haylen::text
