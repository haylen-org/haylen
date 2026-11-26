#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/graphics/Image.hpp"
#include "support/TestFiles.hpp"

namespace haylen::graphics {

class WebpImageTest : public ::testing::Test {
  protected:
    [[nodiscard]] static std::vector<std::uint8_t> file(const std::string& name) {
        return test::TestFiles::read(std::filesystem::path(HAYLEN_TEST_IMAGES) / name);
    }

    // The sentence of the failure of a decode, or an empty text when the bytes decode.
    [[nodiscard]] static std::string refusal(const std::vector<std::uint8_t>& bytes, int maxSide = Image::kMaxSide) {
        try {
            (void)Image::decode(bytes, maxSide, "images/hero.webp");
        } catch (const std::runtime_error& error) {
            return error.what();
        }

        return {};
    }

    // The header of a WebP file of the extended format, which names the size of its canvas and whether it is animated.
    [[nodiscard]] static std::vector<std::uint8_t> extendedHeader(std::uint32_t width, std::uint32_t height, bool animated) {
        std::vector<std::uint8_t> bytes = test::TestFiles::bytes(std::string("RIFF\x16\x00\x00\x00WEBPVP8X\x0A\x00\x00\x00", 20));
        bytes.push_back(animated ? 0x02 : 0x00);
        bytes.insert(bytes.end(), {0, 0, 0});
        for (const std::uint32_t side : {width - 1, height - 1}) {
            bytes.insert(bytes.end(), {static_cast<std::uint8_t>(side), static_cast<std::uint8_t>(side >> 8U), static_cast<std::uint8_t>(side >> 16U)});
        }

        return bytes;
    }
};

// A lossless WebP image, with alpha, decodes to exactly the pixels of the PNG image it was made from.
TEST_F(WebpImageTest, DecodesALosslessImageExactly) {
    const Image reference = Image::decode(file("checker.png"));
    const Image decoded = Image::decode(file("checker-lossless.webp"));
    ASSERT_EQ(decoded.getWidth(), reference.getWidth());
    ASSERT_EQ(decoded.getHeight(), reference.getHeight());
    EXPECT_TRUE(std::ranges::equal(decoded.getPixels(), reference.getPixels()));
}

// A lossy WebP image, opaque or with alpha, decodes to exactly the pixels that the decoder of the pinned libwebp gives on every processor, which the PNG images beside it hold.
TEST_F(WebpImageTest, DecodesLossyImagesWithAndWithoutAlpha) {
    for (const std::string name : {"checker-lossy", "checker-lossy-alpha"}) {
        const Image expected = Image::decode(file(name + ".png"));
        const Image decoded = Image::decode(file(name + ".webp"));
        ASSERT_EQ(decoded.getWidth(), expected.getWidth()) << name;
        ASSERT_EQ(decoded.getHeight(), expected.getHeight()) << name;
        EXPECT_TRUE(std::ranges::equal(decoded.getPixels(), expected.getPixels())) << name;
    }
}

// The size that the header of a WebP file declares is checked before a pixel is allocated, against the largest side of an image and the side the caller names.
TEST_F(WebpImageTest, RefusesAnImageTooLargeBeforeDecodingIt) {
    EXPECT_EQ(refusal(extendedHeader(20000, 16, false)), "The image \"images/hero.webp\" of 20000 by 16 pixels is too large to decode. The image may have at most 16384 pixels on a side.");
    EXPECT_EQ(refusal(file("checker-lossless.webp"), 8), "The image \"images/hero.webp\" of 16 by 16 pixels is too large to decode. The image may have at most 8 pixels on a side.");
}

// An animated WebP image, an AVIF image and damaged data each fail with a sentence that says what they are.
TEST_F(WebpImageTest, RefusesAnimatedAvifAndDamagedImages) {
    EXPECT_EQ(refusal(extendedHeader(16, 16, true)), "The image \"images/hero.webp\" is an animated WebP image, while Haylen decodes still images. Save it as a still image, or play its frames as a sprite sheet.");
    EXPECT_EQ(refusal(test::TestFiles::bytes(std::string("\x00\x00\x00\x1C"
                                                         "ftypavif\x00\x00\x00\x00",
                                                         16))),
              "The image \"images/hero.webp\" is an AVIF image, a format that Haylen does not read. Save it as a PNG, JPEG or WebP image.");

    std::vector<std::uint8_t> truncated = file("checker-lossless.webp");
    truncated.resize(truncated.size() / 2);
    EXPECT_EQ(refusal(truncated), "The image \"images/hero.webp\" is damaged, or it is not a PNG, JPEG, WebP, TGA, BMP or GIF file.");
    EXPECT_EQ(refusal(test::TestFiles::bytes(std::string("RIFF\x04\x00\x00\x00WEBP", 12))), "The image \"images/hero.webp\" is damaged, or it is not a PNG, JPEG, WebP, TGA, BMP or GIF file.");
}

} // namespace haylen::graphics
