#include <gtest/gtest.h>

#include <cmath>
#include <string>

#include "2d/graphics/VectorAtlas.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/VectorImage.hpp"
#include "support/EngineFixture.hpp"
#include "support/TestFiles.hpp"

namespace haylen::graphics2d {

namespace {

class VectorAtlasTest : public ::testing::Test {
  protected:
    static constexpr const char* kSquare = R"(<svg xmlns="http://www.w3.org/2000/svg" width="10" height="10"><rect width="10" height="10" fill="#FF8000"/></svg>)";

    [[nodiscard]] static graphics::VectorImage square() {
        return graphics::VectorImage::parse(test::TestFiles::bytes(kSquare));
    }

    test::EngineFixture fixture;
};

} // namespace

TEST_F(VectorAtlasTest, RoundsScalesUpToQuarterOctaveSteps) {
    EXPECT_EQ(VectorAtlas::stepOf(1.0F), 0);
    EXPECT_EQ(VectorAtlas::stepOf(1.1F), 1);
    EXPECT_EQ(VectorAtlas::stepOf(2.0F), 4);
    EXPECT_EQ(VectorAtlas::stepOf(0.5F), -4);
    EXPECT_FLOAT_EQ(VectorAtlas::scaleOf(4), 2.0F);
    EXPECT_GE(VectorAtlas::scaleOf(VectorAtlas::stepOf(1.37F)), 1.37F);
}

// The first raster of an image is there at once, a new scale draws the nearest raster until its own arrives from the task pool, and rasters of every image share a page.
TEST_F(VectorAtlasTest, MakesTheFirstRasterAtOnceAndTheOthersInTheBackground) {
    VectorAtlas atlas(fixture.engine().getGraphics(), fixture.engine().getJobs());
    const graphics::VectorImage image = square();
    const VectorAtlas::Raster first = atlas.find(image, 1.0F, 1);
    ASSERT_TRUE(first.texture.isValid());
    EXPECT_EQ(first.source.getSize(), math::Vec2(10.0F, 10.0F));
    EXPECT_EQ(atlas.find(image, 1.0F, 1).source, first.source);

    const VectorAtlas::Raster waiting = atlas.find(image, 2.0F, 2);
    EXPECT_EQ(waiting.source, first.source);
    ASSERT_TRUE(fixture.frameUntil([&] { return atlas.find(image, 2.0F, 3).source.width == 20.0F; }));
    EXPECT_EQ(atlas.find(image, 2.0F, 4).texture, first.texture);

    const VectorAtlas::Raster other = atlas.find(square(), 1.0F, 5);
    EXPECT_EQ(other.texture, first.texture);
    EXPECT_NE(other.source, first.source);
    EXPECT_EQ(atlas.getPageCount(), 1U);
}

// Once the atlas holds as many pages as it may, the page that drew longest ago starts over with a new texture, while the pages drawn in the frame stay.
TEST_F(VectorAtlasTest, StartsTheLeastRecentPageOverWhenFull) {
    VectorAtlas atlas(fixture.engine().getGraphics(), fixture.engine().getJobs());
    const float large = static_cast<float>(VectorAtlas::kPageSize) * 0.06F;
    std::vector<graphics::VectorImage> images;
    std::vector<VectorAtlas::Raster> rasters;
    for (std::size_t index = 0; index < VectorAtlas::kMaxPages; ++index) {
        images.push_back(square());
        rasters.push_back(atlas.find(images.back(), large, index + 1));
    }
    EXPECT_EQ(atlas.getPageCount(), VectorAtlas::kMaxPages);

    images.push_back(square());
    const VectorAtlas::Raster newest = atlas.find(images.back(), large, VectorAtlas::kMaxPages + 1);
    EXPECT_EQ(atlas.getPageCount(), VectorAtlas::kMaxPages);
    EXPECT_NE(newest.texture, rasters.front().texture);
    EXPECT_EQ(atlas.find(images[1], large, VectorAtlas::kMaxPages + 1).texture, rasters[1].texture);

    // The image whose page started over makes its raster again.
    EXPECT_NE(atlas.find(images.front(), large, VectorAtlas::kMaxPages + 2).texture, rasters.front().texture);
}

TEST_F(VectorAtlasTest, DrawsVectorImagesFromLua) {
    // clang-format off
    fixture.runLua(R"(
        graphics = require('haylen.graphics') graphics2d = require('haylen.graphics2d') scene = require('haylen.scene')
        icon = graphics.newVectorImage('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><circle cx="8" cy="8" r="8" fill="currentColor"/></svg>')
        scene.push({render = function()
            graphics2d.beginScreen()
            for index = 1, 20 do
                graphics2d.drawVector(icon, index * 30, 100, {width = 8 + index * 4, height = 8 + index * 4, color = '#FF40C0FF', rotation = index * 0.1, layer = 1})
            end
            graphics2d.drawVector(icon, 10, 10)
        end})
    )");
    // clang-format on
    fixture.frames(3);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    EXPECT_EQ(fixture.lua("local stats = graphics2d.stats() return stats.sprites .. ' ' .. stats.drawCalls"), "21 1");

    EXPECT_NE(fixture.lua("graphics2d.beginScreen() graphics2d.drawVector(icon, 0, 0, {source = {0, 0, 4, 4}})").find("Unknown option \"source\""), std::string::npos);
}

} // namespace haylen::graphics2d
