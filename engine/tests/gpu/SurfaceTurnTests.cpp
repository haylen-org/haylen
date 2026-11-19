#include <gtest/gtest.h>

#include <array>

#include "haylen/gpu/SurfaceTurn.hpp"

namespace haylen::gpu {

class SurfaceTurnTest : public ::testing::Test {
  protected:
    static constexpr math::Vec2 kWindow{200.0F, 100.0F};

    // Places a pixel of the window in a viewport the way a device does: through the clip space position that the turned matrix writes and the turned viewport.
    [[nodiscard]] static math::Vec2 place(math::Vec2 pixel, const math::Rect& viewport, int turns) {
        const SurfaceTurn::Matrix identity{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F};
        const SurfaceTurn::Matrix turned = SurfaceTurn::turnClip(identity, turns);
        const math::Vec2 clip{(pixel.x - viewport.x) / viewport.width * 2.0F - 1.0F, 1.0F - (pixel.y - viewport.y) / viewport.height * 2.0F};
        const math::Vec2 moved{turned[0] * clip.x + turned[4] * clip.y, turned[1] * clip.x + turned[5] * clip.y};
        const math::Rect images = SurfaceTurn::turnPixels(viewport, kWindow, turns);
        return {images.x + (moved.x + 1.0F) * 0.5F * images.width, images.y + (1.0F - moved.y) * 0.5F * images.height};
    }
};

TEST_F(SurfaceTurnTest, PlacesEveryPixelWhereTheTurnOfTheImagesTakesIt) {
    const math::Vec2 pixel{30.0F, 20.0F};
    const std::array<math::Vec2, 4> expected{math::Vec2{30.0F, 20.0F}, math::Vec2{80.0F, 30.0F}, math::Vec2{170.0F, 80.0F}, math::Vec2{20.0F, 170.0F}};

    int turns = 0;
    for (const math::Vec2& expectedPlace : expected) {
        for (const math::Rect& viewport : {math::Rect{0.0F, 0.0F, 200.0F, 100.0F}, math::Rect{10.0F, 5.0F, 120.0F, 60.0F}}) {
            const math::Vec2 placed = place(pixel, viewport, turns);
            EXPECT_NEAR(placed.x, expectedPlace.x, 0.001F) << turns;
            EXPECT_NEAR(placed.y, expectedPlace.y, 0.001F) << turns;
        }
        ++turns;
    }
}

TEST_F(SurfaceTurnTest, TurnsRectanglesInsideTheImages) {
    const math::Rect scissor{10.0F, 5.0F, 120.0F, 60.0F};

    EXPECT_EQ(SurfaceTurn::turnPixels(scissor, kWindow, 0), scissor);
    EXPECT_EQ(SurfaceTurn::turnPixels(scissor, kWindow, 1), (math::Rect{35.0F, 10.0F, 60.0F, 120.0F}));
    EXPECT_EQ(SurfaceTurn::turnPixels(scissor, kWindow, 2), (math::Rect{70.0F, 35.0F, 120.0F, 60.0F}));
    EXPECT_EQ(SurfaceTurn::turnPixels(scissor, kWindow, 3), (math::Rect{5.0F, 70.0F, 60.0F, 120.0F}));
}

} // namespace haylen::gpu
