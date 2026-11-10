#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace haylen::test {

// Builds font collections, the `.ttc` files that hold several faces, from font files, as the fonts of the systems ship many of their faces.
class FontCollection final {
  public:
    // Reads a font file of the test data.
    [[nodiscard]] static std::vector<std::uint8_t> read(std::string_view name);

    // Joins the fonts in order after the header of a collection, moving the offsets of the tables of each font to where its tables land.
    [[nodiscard]] static std::vector<std::uint8_t> build(std::span<const std::vector<std::uint8_t>> fonts);

    // Adds a table with the tag and a few bytes of data to the font, as the bitmaps of an emoji face sit beside its outlines.
    [[nodiscard]] static std::vector<std::uint8_t> addTable(std::span<const std::uint8_t> font, std::uint32_t tag);

  private:
    [[nodiscard]] static std::uint32_t readBigEndian(std::span<const std::uint8_t> bytes, std::size_t offset, std::size_t width);
    static void writeBigEndian(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value, std::size_t width);
};

} // namespace haylen::test
