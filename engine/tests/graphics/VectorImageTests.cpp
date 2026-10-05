#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

#include "haylen/graphics/Image.hpp"
#include "haylen/graphics/VectorImage.hpp"
#include "support/EngineFixture.hpp"
#include "support/TestFiles.hpp"

namespace haylen::graphics {

namespace {

class VectorImageTest : public ::testing::Test {
  protected:
    // A view box of 20 by 10 units drawn into a document 40 by 20 pixels wide: a red square on the left, a stroked circle in the color of the draw on the right, and a gradient bar along the bottom inside a moved group.
    static constexpr const char* kIcon = R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="40" height="20" viewBox="0 0 20 10">
        <defs><linearGradient id="fade"><stop offset="0" stop-color="#0000FF"/><stop offset="1" stop-color="#00FF00"/></linearGradient></defs>
        <rect x="0" y="0" width="8" height="8" fill="#FF0000"/>
        <circle cx="15" cy="4" r="3" fill="none" stroke="currentColor" stroke-width="1.5"/>
        <g transform="translate(0 8)"><rect x="0" y="0" width="20" height="2" fill="url(#fade)"/></g>
    </svg>)svg";

    [[nodiscard]] static VectorImage parse(const char* text) {
        return VectorImage::parse(test::TestFiles::bytes(text));
    }
};

} // namespace

TEST_F(VectorImageTest, ReadsTheSizeOfTheDocument) {
    const VectorImage image = parse(kIcon);
    EXPECT_TRUE(image.isValid());
    EXPECT_EQ(image.getSize(), math::Vec2(40.0F, 20.0F));
    EXPECT_NE(image.getId(), parse(kIcon).getId());
    EXPECT_EQ(parse(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 12"><path d="M0 0h24v12z"/></svg>)").getSize(), math::Vec2(24.0F, 12.0F));
    EXPECT_EQ(VectorImage::getRasterSize({40.0F, 20.0F}, 0.33F), std::make_pair(14, 7));
}

TEST_F(VectorImageTest, RasterizesFillsStrokesGradientsAndGroupsAtAScale) {
    const Image raster = parse(kIcon).rasterize(2.0F);
    ASSERT_EQ(raster.getWidth(), 80);
    ASSERT_EQ(raster.getHeight(), 40);

    // Every unit of the view box covers four pixels at a scale of 2.
    EXPECT_EQ(raster.getPixel(10, 10), math::Color::fromHex(0xFF0000FFU));
    EXPECT_EQ(raster.getPixel(60, 4), math::Color::white());
    EXPECT_EQ(raster.getPixel(60, 16).a, 0.0F);
    const math::Color left = raster.getPixel(2, 36);
    const math::Color right = raster.getPixel(77, 36);
    EXPECT_GT(left.b, 0.8F);
    EXPECT_GT(right.g, 0.8F);
    EXPECT_EQ(raster.getPixel(40, 30).a, 0.0F);
}

TEST_F(VectorImageTest, RejectsDocumentsWithoutASize) {
    try {
        (void)parse("not an svg document");
        ADD_FAILURE() << "Text without an SVG document was read.";
    } catch (const std::invalid_argument& error) {
        EXPECT_STREQ(error.what(), "The bytes are not an SVG document with a size. Give its root element a view box, or a width and a height.");
    }
    EXPECT_THROW((void)parse(R"(<svg xmlns="http://www.w3.org/2000/svg"></svg>)"), std::invalid_argument);

    // A document that names no size takes the size of what it draws.
    EXPECT_EQ(parse(R"(<svg xmlns="http://www.w3.org/2000/svg"><rect x="2" width="4" height="3"/></svg>)").getSize(), math::Vec2(4.0F, 3.0F));
}

TEST_F(VectorImageTest, LoadsAndRasterizesFromLua) {
    test::EngineFixture fixture({{"content/icons/icon.svg", kIcon}});
    fixture.runLua("assets = require('haylen.assets') graphics = require('haylen.graphics') icon = assets.vectorImage('icons/icon.svg')");
    EXPECT_EQ(fixture.lua("return icon.width .. ' ' .. icon.height .. ' ' .. tostring(icon == assets.load('icons/icon.svg')) .. ' ' .. assets.typeForPath('icons/icon.svg')"), "40.0 20.0 true vectorImage");
    EXPECT_EQ(fixture.lua("return graphics.newVectorImage('<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 8 4\"/>').width"), "8.0");
    EXPECT_NE(fixture.lua("return graphics.newVectorImage('nothing')").find("The bytes are not an SVG document with a size."), std::string::npos);

    // clang-format off
    fixture.runLua(R"(
        async = require('async')
        async.spawn(function()
            local texture = icon:rasterize(0.5):await()
            rasterized = texture.width .. ' ' .. texture.height .. ' ' .. texture.filter
        end)
    )");
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&fixture] { return fixture.lua("return rasterized") != "nil"; }));
    EXPECT_EQ(fixture.lua("return rasterized"), "20 10 linear");
    EXPECT_NE(fixture.lua("return icon:rasterize(0)").find("A vector image rasterizes at a positive scale"), std::string::npos);
    EXPECT_NE(fixture.lua("return icon:rasterize(1000)").find("fits the maximum texture size"), std::string::npos);
}

} // namespace haylen::graphics
