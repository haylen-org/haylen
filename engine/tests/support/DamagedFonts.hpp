#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace haylen::test {

// Damages the bytes of a TrueType font the way a hostile file would, for the tests of the readers of fonts. The damage to outlines needs a font whose locations of glyphs take two bytes.
class DamagedFonts final {
  public:
    // Moves a table past the end of the file in the table directory.
    static void moveTablePastTheEnd(std::vector<std::uint8_t>& font, std::string_view tag);

    // Turns the outline of a glyph into a composite glyph whose one component is the glyph itself.
    static void makeGlyphOfItself(std::vector<std::uint8_t>& font, std::uint32_t glyph);

    // Points the location of the outline of a glyph past the end of the table of outlines.
    static void moveOutlinePastTheEnd(std::vector<std::uint8_t>& font, std::uint32_t glyph);

    // Makes every subtable of the fourth format of the character map count far more segments than it holds.
    static void overcountSegments(std::vector<std::uint8_t>& font);

    // Makes the header of the horizontal metrics count far more advances than their table holds.
    static void overcountAdvances(std::vector<std::uint8_t>& font);

    // Makes the font count fewer glyphs than its character map names.
    static void shrinkGlyphCount(std::vector<std::uint8_t>& font, std::uint16_t count);

    // Gives the em square no units.
    static void removeEmSquare(std::vector<std::uint8_t>& font);

  private:
    [[nodiscard]] static std::size_t findTable(const std::vector<std::uint8_t>& font, std::string_view tag);
    [[nodiscard]] static std::size_t findOutline(const std::vector<std::uint8_t>& font, std::uint32_t glyph);
    [[nodiscard]] static std::uint16_t readShort(const std::vector<std::uint8_t>& font, std::size_t at);
    static void writeShort(std::vector<std::uint8_t>& font, std::size_t at, std::uint16_t value);
};

} // namespace haylen::test
