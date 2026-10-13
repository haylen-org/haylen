#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <vector>

#include "platform/android/AndroidYuvImage.hpp"

namespace haylen::platform {

TEST(AndroidYuvImageTest, ConvertsPlanarAndSemiPlanarPicturesInsideTheirCrop) {
    // A picture of 4 by 2 pixels in rows of 6, whose crop leaves out the first column: white, blue, blue and white, made of Y, U and V of BT.601.
    const std::vector<std::uint8_t> luma{235, 235, 41, 41, 235, 235, 235, 235, 41, 41, 235, 235};
    const std::vector<std::uint8_t> blue{128, 240, 128};
    const std::vector<std::uint8_t> red{128, 110, 128};
    AndroidYuvImage image{.planes = {{{luma.data(), luma.size(), 6, 1}, {blue.data(), blue.size(), 3, 1}, {red.data(), red.size(), 3, 1}}}, .left = 1, .top = 0, .width = 4, .height = 2};
    std::vector<std::uint8_t> pixels;
    image.toRgba(pixels);
    ASSERT_EQ(pixels.size(), 4U * 2U * 4U);
    EXPECT_EQ(std::vector<std::uint8_t>(pixels.begin(), pixels.begin() + 4), (std::vector<std::uint8_t>{255, 255, 255, 255}));
    EXPECT_NEAR(pixels[4 * 2 + 0], 0, 3);
    EXPECT_NEAR(pixels[4 * 2 + 1], 0, 3);
    EXPECT_NEAR(pixels[4 * 2 + 2], 255, 3);

    // The same picture with U and V interleaved, two bytes apart in one buffer.
    const std::vector<std::uint8_t> chroma{128, 128, 240, 110, 128, 128};
    AndroidYuvImage interleaved{.planes = {{{luma.data(), luma.size(), 6, 1}, {chroma.data(), chroma.size() - 1, 6, 2}, {chroma.data() + 1, chroma.size() - 1, 6, 2}}}, .left = 1, .top = 0, .width = 4, .height = 2};
    std::vector<std::uint8_t> same;
    interleaved.toRgba(same);
    EXPECT_EQ(same, pixels);

    // Planes shorter than the crop needs are refused.
    image.planes[1].size = 1;
    EXPECT_THROW(image.toRgba(pixels), std::invalid_argument);
    image.planes[1].size = blue.size();
    image.width = 6;
    EXPECT_THROW(image.toRgba(pixels), std::invalid_argument);
}

} // namespace haylen::platform
