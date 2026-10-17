#include <gtest/gtest.h>

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

} // namespace haylen::graphics2d
