#include <gtest/gtest.h>

#include "haylen/math/Color.hpp"
#include "platform/linux/LinuxVisualColor.hpp"

namespace haylen::platform {

TEST(LinuxVisualColorTest, PlacesEachChannelInTheBitsOfItsMask) {
    const math::Color splash = math::Color::fromHex(0xE9E6DCFFU);
    EXPECT_EQ(LinuxVisualColor::toPixel(splash, 0xFF0000UL, 0x00FF00UL, 0x0000FFUL), 0xE9E6DCUL);
    EXPECT_EQ(LinuxVisualColor::toPixel(splash, 0x0000FFUL, 0x00FF00UL, 0xFF0000UL), 0xDCE6E9UL);

    // A visual of 16 bits keeps five bits of red and blue and six of green.
    EXPECT_EQ(LinuxVisualColor::toPixel(math::Color::white(), 0xF800UL, 0x07E0UL, 0x001FUL), 0xFFFFUL);
    EXPECT_EQ(LinuxVisualColor::toPixel(math::Color::black(), 0xF800UL, 0x07E0UL, 0x001FUL), 0UL);
    EXPECT_EQ(LinuxVisualColor::toPixel(math::Color{1.0F, 0.0F, 0.0F, 1.0F}, 0xF800UL, 0x07E0UL, 0x001FUL), 0xF800UL);
}

} // namespace haylen::platform
