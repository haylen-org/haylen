#include "support/DamagedFonts.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace haylen::test {

// Each record of the table directory holds the tag, the checksum, the offset and the length of a table.
void DamagedFonts::moveTablePastTheEnd(std::vector<std::uint8_t>& font, std::string_view tag) {
    const std::size_t tables = readShort(font, 4);
    for (std::size_t record = 12; record < 12 + tables * 16; record += 16) {
        if (std::string_view(reinterpret_cast<const char*>(&font[record]), 4) == tag) {
            std::fill_n(font.begin() + static_cast<std::ptrdiff_t>(record + 8), 3, std::uint8_t{0xFF});
            return;
        }
    }
    throw std::invalid_argument("The font has no table " + std::string(tag) + ".");
}

// The outline keeps the count of its contours, set to -1 for a composite glyph, and its box, followed by one component of flags that place it by offsets of one byte, its glyph and its offsets.
void DamagedFonts::makeGlyphOfItself(std::vector<std::uint8_t>& font, std::uint32_t glyph) {
    const std::size_t outline = findOutline(font, glyph);
    writeShort(font, outline, 0xFFFF);
    writeShort(font, outline + 10, 0x0002);
    writeShort(font, outline + 12, static_cast<std::uint16_t>(glyph));
    writeShort(font, outline + 14, 0);
}

// Locations of two bytes count words from the start of the table of outlines, and the outline of a glyph ends where the next one starts.
void DamagedFonts::moveOutlinePastTheEnd(std::vector<std::uint8_t>& font, std::uint32_t glyph) {
    const std::size_t location = findTable(font, "loca") + static_cast<std::size_t>(glyph) * 2;
    writeShort(font, location, 0xFFF0);
    writeShort(font, location + 2, 0xFFFF);
}

// The character map lists its subtables by platform, encoding and offset, and the fourth format holds twice its count of segments in its seventh and eighth bytes.
void DamagedFonts::overcountSegments(std::vector<std::uint8_t>& font) {
    const std::size_t map = findTable(font, "cmap");
    const std::size_t subtables = readShort(font, map + 2);
    for (std::size_t record = map + 4; record < map + 4 + subtables * 8; record += 8) {
        const std::size_t subtable = map + (static_cast<std::size_t>(readShort(font, record + 4)) << 16U | readShort(font, record + 6));
        if (readShort(font, subtable) == 4) {
            writeShort(font, subtable + 6, 0xFFFE);
        }
    }
}

void DamagedFonts::overcountAdvances(std::vector<std::uint8_t>& font) {
    writeShort(font, findTable(font, "hhea") + 34, 0xFFFF);
}

// The table of the maximum profile holds its version and then the count of glyphs.
void DamagedFonts::shrinkGlyphCount(std::vector<std::uint8_t>& font, std::uint16_t count) {
    writeShort(font, findTable(font, "maxp") + 4, count);
}

void DamagedFonts::removeEmSquare(std::vector<std::uint8_t>& font) {
    writeShort(font, findTable(font, "head") + 18, 0);
}

std::size_t DamagedFonts::findTable(const std::vector<std::uint8_t>& font, std::string_view tag) {
    const std::size_t tables = readShort(font, 4);
    for (std::size_t record = 12; record < 12 + tables * 16; record += 16) {
        if (std::string_view(reinterpret_cast<const char*>(&font[record]), 4) == tag) {
            return static_cast<std::size_t>(readShort(font, record + 8)) << 16U | readShort(font, record + 10);
        }
    }
    throw std::invalid_argument("The font has no table " + std::string(tag) + ".");
}

std::size_t DamagedFonts::findOutline(const std::vector<std::uint8_t>& font, std::uint32_t glyph) {
    if (readShort(font, findTable(font, "head") + 50) != 0) {
        throw std::invalid_argument("The font keeps the locations of its glyphs in four bytes.");
    }
    const std::size_t location = findTable(font, "loca") + static_cast<std::size_t>(glyph) * 2;
    return findTable(font, "glyf") + static_cast<std::size_t>(readShort(font, location)) * 2;
}

std::uint16_t DamagedFonts::readShort(const std::vector<std::uint8_t>& font, std::size_t at) {
    return static_cast<std::uint16_t>(font.at(at) << 8U | font.at(at + 1));
}

void DamagedFonts::writeShort(std::vector<std::uint8_t>& font, std::size_t at, std::uint16_t value) {
    font.at(at) = static_cast<std::uint8_t>(value >> 8U);
    font.at(at + 1) = static_cast<std::uint8_t>(value & 0xFFU);
}

} // namespace haylen::test
