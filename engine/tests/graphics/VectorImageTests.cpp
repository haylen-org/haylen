#include <gtest/gtest.h>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include "gpu/NullDevice.hpp"
#include "haylen/graphics/Device.hpp"
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

// A gradient lays itself out in the bounding box of its shape by default, where coordinates without units and percentages are fractions of the box and the gradient transform turns the unit square before the box stretches it.
TEST_F(VectorImageTest, LaysGradientsOutInTheBoundingBoxOfTheirShape) {
    const Image raster = parse(R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="100" height="40">
        <defs>
            <linearGradient id="across" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#0000FF"/><stop offset="1" stop-color="#00FF00"/></linearGradient>
            <linearGradient id="percent" x1="0%" y1="0%" x2="100%" y2="0%"><stop offset="0" stop-color="#0000FF"/><stop offset="1" stop-color="#00FF00"/></linearGradient>
            <linearGradient id="turned" gradientTransform="rotate(90 0.5 0.5)"><stop offset="0" stop-color="#0000FF"/><stop offset="1" stop-color="#00FF00"/></linearGradient>
            <radialGradient id="glow" cx="0.5" cy="0.5" r="0.5"><stop offset="0" stop-color="#FF0000"/><stop offset="1" stop-color="#0000FF"/></radialGradient>
        </defs>
        <rect x="20" y="0" width="20" height="10" fill="url(#across)"/>
        <rect x="60" y="0" width="20" height="10" fill="url(#percent)"/>
        <rect x="0" y="20" width="20" height="20" fill="url(#turned)"/>
        <rect x="40" y="20" width="60" height="20" fill="url(#glow)"/>
    </svg>)svg")
                             .rasterize(1.0F);
    for (const int left : {20, 60}) {
        EXPECT_GT(raster.getPixel(left, 5).b, 0.9F) << "x " << left;
        EXPECT_GT(raster.getPixel(left + 19, 5).g, 0.9F) << "x " << left;
        EXPECT_NEAR(raster.getPixel(left + 10, 5).g, 0.5F, 0.1F) << "x " << left;
    }
    EXPECT_GT(raster.getPixel(10, 20).b, 0.9F);
    EXPECT_GT(raster.getPixel(10, 39).g, 0.9F);

    // The circle of the radial gradient stretches into an ellipse that touches the four sides of its wide box.
    EXPECT_GT(raster.getPixel(70, 30).r, 0.9F);
    EXPECT_GT(raster.getPixel(41, 30).b, 0.9F);
    EXPECT_GT(raster.getPixel(70, 20).b, 0.9F);
    EXPECT_NEAR(raster.getPixel(55, 30).r, 0.5F, 0.1F);
}

// A drawing program leaves hidden shapes in documents, such as the background of a logo, which stay hidden like the shapes that display none.
TEST_F(VectorImageTest, LeavesOutTheShapesThatAreHidden) {
    const Image raster = parse(R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="30" height="10">
        <rect x="0" y="0" width="30" height="10" fill="#FFFFFF" visibility="hidden"/>
        <rect x="10" y="0" width="10" height="10" fill="#FFFFFF" style="visibility: collapse"/>
        <g display="none"><rect x="20" y="0" width="10" height="10" fill="#FFFFFF"/></g>
        <rect x="0" y="0" width="5" height="10" fill="#FF0000"/>
    </svg>)svg")
                             .rasterize(1.0F);
    EXPECT_EQ(raster.getPixel(2, 5), math::Color::fromHex(0xFF0000FFU));
    for (const int x : {8, 15, 25}) {
        EXPECT_EQ(raster.getPixel(x, 5).a, 0.0F) << "x " << x;
    }
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

// A raster whose texture finds no room on the device rejects its promise, so the coroutine that waits for it goes on.
TEST_F(VectorImageTest, RejectsARasterWithoutRoomForItsTexture) {
    test::EngineFixture fixture({{"content/icons/icon.svg", kIcon}});
    fixture.frames(2);
    Device& device = fixture.engine().getGraphics();
    std::vector<Texture> textures;
    try {
        while (textures.size() <= static_cast<std::size_t>(gpu::NullDevice::kImagePoolSize)) {
            textures.push_back(device.createTexture(Image(1, 1, math::Color::white())));
        }
    } catch (const std::runtime_error&) {}

    // clang-format off
    fixture.runLua(R"(
        local icon = require('haylen.assets').vectorImage('icons/icon.svg')
        require('async').spawn(function()
            local texture, error = icon:rasterize(0.5):await()
            failed = tostring(texture) .. ' ' .. tostring(error)
        end)
    )");
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&fixture] { return fixture.lua("return failed") != "nil"; }));
    EXPECT_EQ(fixture.lua("return failed"), "nil The graphics device has no room for another texture. At most 4096 textures and render targets can exist at once.");
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

} // namespace haylen::graphics
