#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>

#include "2d/graphics/GpuInstance.hpp"
#include "graphics/TextureResource.hpp"
#include "haylen/math/Math.hpp"

namespace haylen::graphics2d {

// The sprite shaders shorten a tilted quad by the fourth parameter byte, so the byte holds how much the cosine of the tilt takes from its height, the same for a tilt toward or away from the viewer.
TEST(GpuInstanceTest, PacksHowMuchATiltShortensASprite) {
    graphics::TextureResource resource;
    resource.width = 16;
    resource.height = 16;
    EXPECT_EQ(GpuInstance::make(resource, {}).parameters[3], 0);
    EXPECT_EQ(GpuInstance::make(resource, {.tilt = math::Math::kPi / 3.0F}).parameters[3], 128);
    EXPECT_EQ(GpuInstance::make(resource, {.tilt = -math::Math::kPi / 3.0F}).parameters[3], 128);
    EXPECT_EQ(GpuInstance::make(resource, {.tilt = math::Math::kPi / 2.0F}).parameters[3], 255);
}

// A source inside the texture keeps all 16 bits for each coordinate, and a source past the texture spreads them over the smallest range of whole textures that holds it, from 0, or centered on 0 when it starts before the texture, which the sprite shaders read back from the bits of the first parameter byte above the flips.
TEST(GpuInstanceTest, PacksSourcesThatReachPastTheTexture) {
    graphics::TextureResource resource;
    resource.width = 64;
    resource.height = 64;
    // clang-format off
    const auto decode = [](const GpuInstance& packed) {
        const int range = (packed.parameters[0] >> 3U) & 0x0FU;
        const float span = std::ldexp(1.0F, range);
        const float shift = (packed.parameters[0] & 0x80U) != 0 ? span * 0.5F : 0.0F;
        std::array<float, 4> corners{};
        for (std::size_t index = 0; index < corners.size(); ++index) {
            corners[index] = static_cast<float>(packed.uv[index]) / 65535.0F * span - shift;
        }
        return corners;
    };
    // clang-format on

    const GpuInstance inside = GpuInstance::make(resource, {.source = {16.0F, 0.0F, 32.0F, 64.0F}, .flip = {.horizontal = true}});
    EXPECT_EQ(inside.parameters[0], 1U);
    EXPECT_EQ(inside.uv[0], 16384U);
    EXPECT_EQ(inside.uv[2], 49151U);

    // Ten copies across and two down need a range of 16 textures, which still flips.
    const GpuInstance repeated = GpuInstance::make(resource, {.source = {0.0F, 0.0F, 640.0F, 128.0F}, .flip = {.horizontal = true}});
    EXPECT_EQ(repeated.parameters[0] & 0x07U, 1U);
    const std::array<float, 4> wide = decode(repeated);
    EXPECT_NEAR(wide[0], 0.0F, 1e-3F);
    EXPECT_NEAR(wide[1], 0.0F, 1e-3F);
    EXPECT_NEAR(wide[2], 10.0F, 1e-3F);
    EXPECT_NEAR(wide[3], 2.0F, 1e-3F);

    // A source that starts before the texture centers its range on 0.
    const std::array<float, 4> early = decode(GpuInstance::make(resource, {.source = {-96.0F, -32.0F, 160.0F, 64.0F}}));
    EXPECT_NEAR(early[0], -1.5F, 1e-3F);
    EXPECT_NEAR(early[1], -0.5F, 1e-3F);
    EXPECT_NEAR(early[2], 1.0F, 1e-3F);
    EXPECT_NEAR(early[3], 0.5F, 1e-3F);
}

} // namespace haylen::graphics2d
