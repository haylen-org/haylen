#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "graphics/Gpu.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/BlendMode.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "support/EngineFixture.hpp"

namespace haylen {

TEST(ImageTest, CreatesDecodesAndEditsPixels) {
    graphics::Image image(4, 2, math::Color::fromHex(0xFF0000FFU));
    EXPECT_EQ(image.getWidth(), 4);
    EXPECT_EQ(image.getHeight(), 2);
    EXPECT_FALSE(image.isEmpty());
    EXPECT_EQ(image.getPixel(3, 1), math::Color::fromHex(0xFF0000FFU));
    EXPECT_EQ(image.getPixel(9, 9), math::Color::transparent());

    image.setPixel(1, 1, math::Color::white());
    image.setPixel(-1, 0, math::Color::white());
    EXPECT_EQ(image.getPixel(1, 1), math::Color::white());

    const graphics::Image decoded = graphics::Image::decode(test::pngImage(3, 5, 0x00FF00FFU));
    EXPECT_EQ(decoded.getWidth(), 3);
    EXPECT_EQ(decoded.getPixel(2, 4), math::Color::fromHex(0x00FF00FFU));
    EXPECT_THROW((void)graphics::Image::decode(test::bytes("not an image")), std::runtime_error);
    EXPECT_THROW(graphics::Image(2, 2, std::vector<std::uint8_t>(3)), std::invalid_argument);
    EXPECT_THROW(graphics::Image(-1, 2), std::invalid_argument);
}

TEST(ImageTest, BlitsCropsAndFindsOpaqueBounds) {
    graphics::Image canvas(8, 8);
    canvas.blit(graphics::Image(2, 3, math::Color::white()), 3, 2);
    canvas.blit(graphics::Image(4, 4, math::Color::white()), 20, 20);
    EXPECT_EQ(canvas.getOpaqueBounds(), (math::Rect{3.0F, 2.0F, 2.0F, 3.0F}));
    EXPECT_EQ(graphics::Image(4, 4).getOpaqueBounds(), math::Rect{});

    const graphics::Image cropped = canvas.crop({3.0F, 2.0F, 2.0F, 3.0F});
    EXPECT_EQ(cropped.getWidth(), 2);
    EXPECT_EQ(cropped.getPixel(1, 2), math::Color::white());
}

TEST(DeviceTest, CreatesReplacesAndReleasesResources) {
    test::EngineFixture fixture;
    graphics::Device& device = fixture.engine().getGraphics();
    EXPECT_EQ(device.getBackendName(), "dummy");
    EXPECT_EQ(device.getMaxTextureSize(), 16384);

    // The headless host takes the limits of desktop GPUs, so strips wider than 1024 pixels, like the foam of Tiny Island, load in tests.
    EXPECT_EQ(device.createTexture(graphics::Image(3072, 4)).getWidth(), 3072);
    EXPECT_EQ(device.createRenderTarget(4096, 2048).getWidth(), 4096);

    const graphics::Texture texture = device.createTexture(graphics::Image(4, 2, math::Color::white()), {.filter = graphics::Texture::Filter::Linear, .wrap = graphics::Texture::Wrap::Repeat});
    EXPECT_TRUE(texture.isValid());
    EXPECT_EQ(texture.getSize(), math::Vec2(4.0F, 2.0F));
    EXPECT_EQ(texture.getOptions(), (graphics::Texture::Options{.filter = graphics::Texture::Filter::Linear, .wrap = graphics::Texture::Wrap::Repeat}));
    EXPECT_NE(texture.getId(), 0U);

    device.replaceTexture(texture, graphics::Image(8, 8));
    EXPECT_EQ(texture.getWidth(), 8);

    const graphics::Texture alpha = device.createAlphaTexture(2, 2, std::vector<std::uint8_t>(4, 255));
    device.replaceAlphaTexture(alpha, 4, 4, std::vector<std::uint8_t>(16, 0));
    EXPECT_EQ(alpha.getHeight(), 4);
    EXPECT_THROW((void)device.createAlphaTexture(2, 2, std::vector<std::uint8_t>(3)), std::invalid_argument);
    EXPECT_THROW((void)device.createTexture(graphics::Image(0, 0)), std::invalid_argument);
    EXPECT_THROW((void)device.createRenderTarget(1, 1 << 20), std::invalid_argument);

    const graphics::RenderTarget target = device.createRenderTarget(64, 32);
    EXPECT_TRUE(target.isValid());
    EXPECT_EQ(target.getSize(), math::Vec2(64.0F, 32.0F));
    EXPECT_FALSE(graphics::RenderTarget{}.getTexture().isValid());

    const graphics::Texture empty;
    EXPECT_EQ(empty.getWidth(), 0);
    EXPECT_EQ(empty.getHeight(), 0);
    EXPECT_EQ(empty.getId(), 0U);
    EXPECT_EQ(empty.getOptions(), graphics::Texture::Options{});
    device.collectGarbage();
}

TEST(DeviceTest, ReportsAFullTexturePool) {
    test::EngineFixture fixture;
    graphics::Device& device = fixture.engine().getGraphics();
    const graphics::Image pixel(1, 1, math::Color::white());
    std::vector<graphics::Texture> textures;
    std::string error;
    try {
        while (textures.size() <= static_cast<std::size_t>(graphics::Gpu::kImagePoolSize)) {
            textures.push_back(device.createTexture(pixel));
        }
    } catch (const std::runtime_error& failure) {
        error = failure.what();
    }
    EXPECT_NE(error.find("no room for another texture"), std::string::npos);
    EXPECT_LT(textures.size(), static_cast<std::size_t>(graphics::Gpu::kImagePoolSize));

    // A replacement that finds the pool full keeps the texture it was replacing.
    EXPECT_THROW(device.replaceTexture(textures.front(), graphics::Image(2, 2, math::Color::white())), std::runtime_error);
    EXPECT_EQ(textures.front().getWidth(), 1);

    textures.clear();
    device.collectGarbage();
    EXPECT_TRUE(device.createTexture(pixel).isValid());
}

TEST(GraphicsNamesTest, ParsesBlendFilterAndWrapNames) {
    EXPECT_EQ(graphics::BlendMode::parse("additive"), graphics::BlendMode::Type::Additive);
    EXPECT_EQ(graphics::BlendMode::name(graphics::BlendMode::Type::Screen), "screen");
    EXPECT_FALSE(graphics::BlendMode::parse("burn").has_value());
    EXPECT_EQ(graphics::Texture::filterFromName("linear"), graphics::Texture::Filter::Linear);
    EXPECT_EQ(graphics::Texture::filterFromName("nearest"), graphics::Texture::Filter::Nearest);
    EXPECT_FALSE(graphics::Texture::filterFromName("cubic").has_value());
    EXPECT_EQ(graphics::Texture::wrapFromName("mirror"), graphics::Texture::Wrap::Mirror);
    EXPECT_EQ(graphics::Texture::wrapFromName("clamp"), graphics::Texture::Wrap::Clamp);
    EXPECT_EQ(graphics::Texture::wrapFromName("repeat"), graphics::Texture::Wrap::Repeat);
    EXPECT_FALSE(graphics::Texture::wrapFromName("border").has_value());
}

} // namespace haylen
