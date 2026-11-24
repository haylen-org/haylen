#include <gtest/gtest.h>

#include <string>

#include "support/LuaFixture.hpp"

namespace haylen::lua::graphics {

TEST(ImagePaletteLuaTest, ReadsThePaletteSwatchesAndPlaceholdersOfAnImage) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        graphics = require('haylen.graphics')
        cover = graphics.newImage(120, 100)
        cover:fill('#FF1E3A8A', {10, 10, 100, 50})
        cover:fill('#FFF97316', {10, 60, 100, 25})
        cover:fill('#FFE8E2D6', {10, 85, 100, 10})
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("local palette = cover:palette() return #palette .. ' ' .. palette[1].color:toHex() .. ' ' .. string.format('%.2f', palette[1].population)"), "3 #FF1E3A8A 0.58");
    EXPECT_EQ(fixture.lua("local swatches = cover:swatches() return swatches.dominant:toHex() .. ' ' .. swatches.vibrant:toHex() .. ' ' .. tostring(swatches.muted)"), "#FF1E3A8A #FFF97316 nil");
    EXPECT_EQ(fixture.lua("local code = cover:placeholder({columns = 3, rows = 2}) local image = graphics.decodePlaceholder(code, 30, 20) return #code .. ' ' .. image.width .. ' ' .. image.height"), "20 30 20");
    EXPECT_NE(fixture.lua("cover:placeholder({colums = 3})").find("Unknown option \"colums\"."), std::string::npos);
    EXPECT_NE(fixture.lua("graphics.decodePlaceholder('nope!', 8, 8)").find("The text is not an image placeholder."), std::string::npos);
    EXPECT_NE(fixture.lua("cover:palette(0)").find("A palette has from 1 to 64 colors, not 0."), std::string::npos);
    EXPECT_NE(fixture.lua("cover:palette(4294967297)").find("A palette has from 1 to 64 colors, not 4294967297."), std::string::npos);
    EXPECT_NE(fixture.lua("cover:swatches(-4294967288)").find("A palette has from 1 to 64 colors, not -4294967288."), std::string::npos);
}

} // namespace haylen::lua::graphics
