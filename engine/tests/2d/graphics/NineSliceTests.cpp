#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "2d/graphics/GpuInstance.hpp"
#include "graphics/TextureResource.hpp"
#include "haylen/2d/graphics/NineSlice.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/math/Insets.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::graphics2d {

namespace {

class NineSliceTest : public ::testing::Test {
  protected:
    [[nodiscard]] graphics::Texture makeTexture(int width, int height) {
        return fixture.engine().getGraphics().createTexture(graphics::Image(width, height));
    }

    [[nodiscard]] static std::vector<NineSlice::Patch> layout(const NineSlice& slice, const math::Rect& area, float borderScale = 1.0F) {
        std::vector<NineSlice::Patch> patches;
        slice.layout(area, borderScale, patches);
        return patches;
    }

    test::EngineFixture fixture;
};

} // namespace

TEST_F(NineSliceTest, BuildsPiecesFromBorders) {
    const graphics::Texture texture = makeTexture(30, 20);
    const NineSlice slice = NineSlice::fromBorders(texture, {}, {5.0F, 4.0F, 6.0F, 3.0F});
    EXPECT_EQ(slice.pieces[0], (math::Rect{0.0F, 0.0F, 5.0F, 4.0F}));
    EXPECT_EQ(slice.pieces[4], (math::Rect{5.0F, 4.0F, 19.0F, 13.0F}));
    EXPECT_EQ(slice.pieces[8], (math::Rect{24.0F, 17.0F, 6.0F, 3.0F}));
    EXPECT_EQ(slice.getBorders(), (math::Insets{5.0F, 4.0F, 6.0F, 3.0F}));
    EXPECT_TRUE(slice.isValid());

    std::array<math::Rect, 9> pieces{};
    pieces.fill({0.0F, 0.0F, 2.0F, 2.0F});
    EXPECT_EQ(NineSlice::fromPieces(texture, pieces).getBorders(), math::Insets::uniform(2.0F));
}

TEST_F(NineSliceTest, RejectsBordersThatDoNotFitTheirSource) {
    const graphics::Texture texture = makeTexture(30, 20);
    EXPECT_THROW((void)NineSlice::fromBorders(texture, {}, {16.0F, 0.0F, 16.0F, 0.0F}), std::invalid_argument);
    EXPECT_THROW((void)NineSlice::fromBorders(texture, {0.0F, 0.0F, 10.0F, 10.0F}, {0.0F, 6.0F, 0.0F, 6.0F}), std::invalid_argument);
    EXPECT_THROW((void)NineSlice::fromBorders(texture, {}, {-1.0F, 0.0F, 0.0F, 0.0F}), std::invalid_argument);
    EXPECT_NO_THROW((void)NineSlice::fromBorders(texture, {}, {15.0F, 10.0F, 15.0F, 10.0F}));
}

// Separate pieces of different sizes, such as those an atlas packs, give each side the size of its largest piece.
TEST_F(NineSliceTest, TakesEachBorderFromTheLargestPieceOfItsSide) {
    const std::array<math::Rect, 9> pieces{
        math::Rect{0.0F, 0.0F, 6.0F, 5.0F}, math::Rect{10.0F, 0.0F, 4.0F, 3.0F}, math::Rect{20.0F, 0.0F, 2.0F, 2.0F}, math::Rect{0.0F, 10.0F, 4.0F, 4.0F}, math::Rect{10.0F, 10.0F, 4.0F, 4.0F}, math::Rect{20.0F, 10.0F, 7.0F, 4.0F}, math::Rect{0.0F, 20.0F, 3.0F, 1.0F}, math::Rect{10.0F, 20.0F, 4.0F, 8.0F}, math::Rect{20.0F, 20.0F, 2.0F, 2.0F},
    };
    EXPECT_EQ(NineSlice::fromPieces(makeTexture(32, 32), pieces).getBorders(), (math::Insets{.left = 6.0F, .top = 5.0F, .right = 7.0F, .bottom = 8.0F}));
}

TEST_F(NineSliceTest, LaysOutCornersEdgesAndCenterAtTheBorderScale) {
    const NineSlice slice = NineSlice::fromBorders(makeTexture(30, 20), {}, {5.0F, 4.0F, 6.0F, 3.0F});
    const std::vector<NineSlice::Patch> patches = layout(slice, {10.0F, 20.0F, 100.0F, 50.0F}, 2.0F);
    ASSERT_EQ(patches.size(), 9U);
    EXPECT_EQ(patches[0].area, (math::Rect{10.0F, 20.0F, 10.0F, 8.0F}));
    EXPECT_EQ(patches[4].area, (math::Rect{20.0F, 28.0F, 78.0F, 36.0F}));
    EXPECT_EQ(patches[8].area, (math::Rect{98.0F, 64.0F, 12.0F, 6.0F}));
    for (std::size_t index = 0; index < patches.size(); ++index) {
        EXPECT_EQ(patches[index].source, slice.pieces[index]);
    }
}

// The borders of an axis shrink together until they meet, and the pieces between them draw nothing.
TEST_F(NineSliceTest, ShrinksBordersThatDoNotFitTheArea) {
    const NineSlice slice = NineSlice::fromBorders(makeTexture(30, 20), {}, {5.0F, 4.0F, 6.0F, 3.0F});
    const std::vector<NineSlice::Patch> patches = layout(slice, {0.0F, 0.0F, 11.0F, 100.0F}, 2.0F);
    ASSERT_EQ(patches.size(), 6U);
    EXPECT_EQ(patches[0].area, (math::Rect{0.0F, 0.0F, 5.0F, 8.0F}));
    EXPECT_EQ(patches[1].area, (math::Rect{5.0F, 0.0F, 6.0F, 8.0F}));
    EXPECT_EQ(patches[2].area, (math::Rect{0.0F, 8.0F, 5.0F, 86.0F}));
    EXPECT_EQ(patches[2].source, slice.pieces[3]);
    EXPECT_EQ(patches[5].area, (math::Rect{5.0F, 94.0F, 6.0F, 6.0F}));

    EXPECT_TRUE(layout(slice, {0.0F, 0.0F, 0.0F, 10.0F}).empty());
    EXPECT_TRUE(layout(slice, {0.0F, 0.0F, 10.0F, -5.0F}).empty());
    std::vector<NineSlice::Patch> ignored;
    EXPECT_THROW(slice.layout({0.0F, 0.0F, 10.0F, 10.0F}, 0.0F, ignored), std::invalid_argument);
    EXPECT_THROW(slice.layout({0.0F, 0.0F, 10.0F, 10.0F}, std::nanf(""), ignored), std::invalid_argument);
}

// The edges repeat along their length and the center on both axes, and every last copy shows the start of its piece.
TEST_F(NineSliceTest, TilesEdgesAndCenterAndCropsTheLastCopies) {
    NineSlice slice = NineSlice::fromBorders(makeTexture(12, 12), {}, math::Insets::uniform(4.0F));
    slice.fill = NineSlice::Fill::Tile;
    const std::vector<NineSlice::Patch> patches = layout(slice, {0.0F, 0.0F, 30.0F, 30.0F});
    ASSERT_EQ(patches.size(), 4U + 4U * 6U + 36U);

    const NineSlice::Patch& topLast = patches[6];
    EXPECT_EQ(topLast.area, (math::Rect{24.0F, 0.0F, 2.0F, 4.0F}));
    EXPECT_EQ(topLast.source, (math::Rect{4.0F, 0.0F, 2.0F, 4.0F}));
    const NineSlice::Patch& centerLast = patches[8 + 6 + 35];
    EXPECT_EQ(centerLast.area, (math::Rect{24.0F, 24.0F, 2.0F, 2.0F}));
    EXPECT_EQ(centerLast.source, (math::Rect{4.0F, 4.0F, 2.0F, 2.0F}));
}

// A tiled edge piece thinner than its border stretches across the border and keeps its shape along it, instead of repeating across it.
TEST_F(NineSliceTest, KeepsTheShapeOfTiledEdgesAcrossTheirBorder) {
    std::array<math::Rect, 9> pieces{};
    pieces.fill({0.0F, 0.0F, 4.0F, 4.0F});
    pieces[1] = {4.0F, 0.0F, 4.0F, 2.0F};
    NineSlice slice = NineSlice::fromPieces(makeTexture(12, 12), pieces);
    slice.fill = NineSlice::Fill::Tile;

    std::vector<NineSlice::Patch> top;
    for (const NineSlice::Patch& patch : layout(slice, {0.0F, 0.0F, 24.0F, 24.0F})) {
        if (patch.source.height == 2.0F) {
            top.push_back(patch);
        }
    }
    ASSERT_EQ(top.size(), 2U);
    EXPECT_EQ(top[0].area, (math::Rect{4.0F, 0.0F, 8.0F, 4.0F}));
    EXPECT_EQ(top[1].area, (math::Rect{12.0F, 0.0F, 8.0F, 4.0F}));
}

// A length that is a whole number of tiles up to the error of floats draws no sliver of an extra tile.
TEST_F(NineSliceTest, LeavesOutSliversOfRoundingError) {
    NineSlice slice = NineSlice::fromBorders(makeTexture(10, 10), {}, {});
    slice.fill = NineSlice::Fill::Tile;
    const std::vector<NineSlice::Patch> patches = layout(slice, {0.0F, 0.0F, 0.3F * 10.0F + 1e-6F, 1.0F}, 0.1F);
    ASSERT_EQ(patches.size(), 3U);
    for (const NineSlice::Patch& patch : patches) {
        EXPECT_GT(patch.area.width, 0.99F);
    }
}

// Pieces in pixels become texture coordinates of their texture at every size, and the rows of render targets that start at the bottom flip.
TEST_F(NineSliceTest, TurnsPiecesIntoTextureCoordinatesAtEverySize) {
    const auto decode = [](std::uint16_t value, int size) { return static_cast<float>(value) / 65535.0F * static_cast<float>(size); };
    for (const int size : {16, 1024, 4096, 16384}) {
        graphics::TextureResource resource;
        resource.width = size;
        resource.height = size / 2;
        const math::Rect piece{static_cast<float>(size) * 0.25F + 1.0F, 3.0F, 7.0F, static_cast<float>(size / 4)};
        const GpuInstance packed = GpuInstance::make(resource, {.source = piece});
        EXPECT_NEAR(decode(packed.uv[0], size), piece.getLeft(), 0.25F) << size;
        EXPECT_NEAR(decode(packed.uv[2], size), piece.getRight(), 0.25F) << size;
        EXPECT_NEAR(decode(packed.uv[1], size / 2), piece.getTop(), 0.25F) << size;
        EXPECT_NEAR(decode(packed.uv[3], size / 2), piece.getBottom(), 0.25F) << size;

        resource.flipped = true;
        const GpuInstance flipped = GpuInstance::make(resource, {.source = piece});
        EXPECT_NEAR(decode(flipped.uv[1], size / 2), static_cast<float>(size / 2) - piece.getTop(), 0.25F) << size;
        EXPECT_NEAR(decode(flipped.uv[3], size / 2), static_cast<float>(size / 2) - piece.getBottom(), 0.25F) << size;
    }
}

} // namespace haylen::graphics2d
