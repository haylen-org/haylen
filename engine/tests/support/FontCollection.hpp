#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace haylen::test {

// Builds fonts from the font files of the test data: font collections, the `.ttc` files that hold several faces, as the fonts of the systems ship many of their faces, and fonts whose tables a test changes.
class FontCollection final {
  public:
    // Reads a font file of the test data.
    [[nodiscard]] static std::vector<std::uint8_t> read(std::string_view name);

    // Joins the fonts in order after the header of a collection, moving the offsets of the tables of each font to where its tables land.
    [[nodiscard]] static std::vector<std::uint8_t> build(std::span<const std::vector<std::uint8_t>> fonts);

    // Adds a table with the tag and a few bytes of data to the font, as the bitmaps of an emoji face sit beside its outlines.
    [[nodiscard]] static std::vector<std::uint8_t> addTable(std::span<const std::uint8_t> font, std::uint32_t tag);

    // Gives the font other line metrics in font units, in its horizontal header and in the typographic and Windows metrics of its OS/2 table, so its glyphs may reach past its lines, as the stacked marks of some scripts do.
    [[nodiscard]] static std::vector<std::uint8_t> setLineMetrics(std::span<const std::uint8_t> font, std::int16_t ascender, std::int16_t descender, std::int16_t lineGap);

  private:
    [[nodiscard]] static std::size_t findTable(std::span<const std::uint8_t> font, std::uint32_t tag);
    [[nodiscard]] static std::uint32_t readBigEndian(std::span<const std::uint8_t> bytes, std::size_t offset, std::size_t width);
    static void writeBigEndian(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value, std::size_t width);
};

} // namespace haylen::test
