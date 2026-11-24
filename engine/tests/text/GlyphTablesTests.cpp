#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "support/FontCollection.hpp"
#include "text/FontFile.hpp"
#include "text/GlyphTables.hpp"

namespace haylen::text {

// The engine draws a face through its color layers, or through its outlines unless bitmaps hold its real glyphs, which HarfBuzz reads from the tables of the face, in a collection too.
TEST(GlyphTablesTest, DrawsColorLayersAndOutlinesWithoutBitmaps) {
    EXPECT_TRUE(GlyphTables::isDrawable(std::array<std::uint32_t, 2>{0x636D6170U, 0x676C7966U}));
    EXPECT_TRUE(GlyphTables::isDrawable(std::array<std::uint32_t, 2>{0x43464620U, 0x636D6170U}));
    EXPECT_TRUE(GlyphTables::isDrawable(std::array<std::uint32_t, 3>{0x43424454U, 0x434F4C52U, 0x676C7966U}));
    EXPECT_FALSE(GlyphTables::isDrawable(std::array<std::uint32_t, 2>{0x43424454U, 0x676C7966U}));
    EXPECT_FALSE(GlyphTables::isDrawable(std::array<std::uint32_t, 2>{0x676C7966U, 0x73626978U}));
    EXPECT_FALSE(GlyphTables::isDrawable(std::array<std::uint32_t, 1>{0x636D6170U}));

    const std::vector<std::uint8_t> letters = test::FontCollection::read("crimson-text-regular.ttf");
    const std::vector<std::uint8_t> bitmaps = test::FontCollection::addTable(letters, 0x43424454U);
    const std::vector<std::vector<std::uint8_t>> faces{letters, bitmaps};
    const std::vector<std::uint8_t> collection = test::FontCollection::build(faces);
    EXPECT_EQ(FontFile::countFaces(collection), 2U);
    const std::optional<FontFile> first = FontFile::read(std::span<const std::uint8_t>(collection), 0);
    const std::optional<FontFile> second = FontFile::read(std::span<const std::uint8_t>(collection), 1);
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    EXPECT_TRUE(first->isDrawable());
    EXPECT_FALSE(second->isDrawable());
    EXPECT_FALSE(FontFile::read(std::span<const std::uint8_t>(collection), 2).has_value());
}

} // namespace haylen::text
