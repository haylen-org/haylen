#include <gtest/gtest.h>

#include "platform/PixelDensityCap.hpp"

namespace haylen::platform {

// A display denser than the cap shrinks the drawable by the cap over its density, at least one pixel on each side, while a cap of 0, a display that is not denser and a window without pixels keep every pixel.
TEST(PixelDensityCapTest, ShrinksTheDrawableOfADenserDisplayToTheCap) {
    EXPECT_EQ(PixelDensityCap::apply(1179, 2556, 3.0F, 2.0F), (gpu::Surface::Size{.width = 786, .height = 1704, .windowWidth = 1179, .windowHeight = 2556}));
    EXPECT_EQ(PixelDensityCap::apply(3840, 2160, 2.0F, 1.0F), (gpu::Surface::Size{.width = 1920, .height = 1080, .windowWidth = 3840, .windowHeight = 2160}));
    EXPECT_EQ(PixelDensityCap::apply(4, 2, 4.0F, 0.1F), (gpu::Surface::Size{.width = 1, .height = 1, .windowWidth = 4, .windowHeight = 2}));
    EXPECT_FALSE(PixelDensityCap::apply(1920, 1080, 2.0F, 0.0F).isScaled());
    EXPECT_FALSE(PixelDensityCap::apply(1920, 1080, 2.0F, 2.0F).isScaled());
    EXPECT_FALSE(PixelDensityCap::apply(1920, 1080, 1.0F, 1.5F).isScaled());
    EXPECT_FALSE(PixelDensityCap::apply(0, 0, 3.0F, 1.0F).isScaled());

    EXPECT_FLOAT_EQ(PixelDensityCap::getShare(PixelDensityCap::apply(3840, 2160, 2.0F, 1.0F)), 0.5F);
    EXPECT_FLOAT_EQ(PixelDensityCap::getShare(PixelDensityCap::apply(3840, 2160, 2.0F, 0.0F)), 1.0F);
}

} // namespace haylen::platform
