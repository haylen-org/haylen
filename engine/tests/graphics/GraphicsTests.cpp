#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "graphics/Gpu.hpp"
#include "graphics/TextureResource.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/BlendMode.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/math/Insets.hpp"
#include "support/AllocationTracker.hpp"
#include "support/EngineFixture.hpp"
#include "support/TestFiles.hpp"

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

    const graphics::Image decoded = graphics::Image::decode(test::TestFiles::pngImage(3, 5, 0x00FF00FFU));
    EXPECT_EQ(decoded.getWidth(), 3);
    EXPECT_EQ(decoded.getPixel(2, 4), math::Color::fromHex(0x00FF00FFU));
    EXPECT_THROW((void)graphics::Image::decode(test::TestFiles::bytes("not an image")), std::runtime_error);
    EXPECT_THROW(graphics::Image(2, 2, std::vector<std::uint8_t>(3)), std::invalid_argument);
    EXPECT_THROW(graphics::Image(-1, 2), std::invalid_argument);

    // A size whose bytes no pointer difference can span fails before any pixel is allocated.
    EXPECT_THROW(graphics::Image(std::numeric_limits<int>::max(), std::numeric_limits<int>::max()), std::invalid_argument);
}

TEST(ImageTest, FillsBlitsCropsAndExtrudesAreas) {
    const math::Color red = math::Color::fromHex(0xFF0000FFU);
    const math::Color blue = math::Color::fromHex(0x0000FFFFU);
    graphics::Image image(8, 8);
    image.fill({-2, -2, 4, 4}, red);
    EXPECT_EQ(image.getPixel(1, 1), red);
    EXPECT_EQ(image.getPixel(2, 2), math::Color::transparent());

    // A blit clips to both images, and blending mixes the straight colors by alpha.
    graphics::Image stamp(4, 4, blue.withAlpha(0.5F));
    image.blit(stamp, {0, 0, 4, 4}, 6, -2);
    EXPECT_EQ(image.getPixel(7, 1).toRgba8(), stamp.getPixel(0, 0).toRgba8());
    EXPECT_EQ(image.getPixel(5, 1), math::Color::transparent());
    image.blit(stamp, {0, 0, 1, 1}, 1, 1, graphics::Image::BlitMode::Blend);
    const math::Color mixed = image.getPixel(1, 1);
    EXPECT_NEAR(mixed.r, 0.5F, 0.01F);
    EXPECT_NEAR(mixed.b, 0.5F, 0.01F);
    EXPECT_NEAR(mixed.a, 1.0F, 0.01F);
    image.blit(image, {0, 0, 2, 2}, 1, 0);
    EXPECT_EQ(image.getPixel(1, 0), red);

    const graphics::Image corner = image.crop({0, 0, 2, 2});
    EXPECT_EQ(corner.getWidth(), 2);
    EXPECT_EQ(corner.getPixel(0, 0), red);
    EXPECT_THROW((void)image.crop({6, 6, 4, 4}), std::out_of_range);

    graphics::Image sheet(10, 10);
    sheet.fill({3, 3, 4, 4}, red);
    sheet.setPixel(6, 6, blue);
    sheet.extrude({3, 3, 4, 4}, 2);
    EXPECT_EQ(sheet.getPixel(1, 3), red);
    EXPECT_EQ(sheet.getPixel(8, 8), blue);
    EXPECT_EQ(sheet.getPixel(0, 0), math::Color::transparent());
    EXPECT_EQ(sheet.getVisibleArea(), (graphics::Image::Area{1, 1, 8, 8}));
    EXPECT_EQ(graphics::Image(3, 3).getVisibleArea(), graphics::Image::Area{});
}

TEST(ImageTest, BleedsColorsIntoTransparentPixelsAndHalvesByAlpha) {
    graphics::Image image(6, 1);
    image.setPixel(0, 0, math::Color::fromHex(0xFF0000FFU));
    image.setPixel(5, 0, math::Color::fromHex(0x0000FFFFU));
    image.bleed();
    EXPECT_EQ(image.getPixels()[4 * 1 + 0], 255);
    EXPECT_EQ(image.getPixels()[4 * 1 + 3], 0);
    EXPECT_EQ(image.getPixels()[4 * 4 + 2], 255);
    EXPECT_EQ(image.getPixels()[4 * 4 + 3], 0);

    // Bleeding reaches a few pixels around what shows.
    graphics::Image strip(12, 1, math::Color::fromRgba8(0, 0, 0, 0));
    strip.setPixel(0, 0, math::Color::fromHex(0xFFFFFFFFU));
    strip.bleed();
    EXPECT_EQ(strip.getPixels()[4 * graphics::Image::kBleedReach], 255);
    EXPECT_EQ(strip.getPixels()[4 * (graphics::Image::kBleedReach + 1)], 0);

    // A pixel that shows keeps its color in the smaller level, whatever color its transparent neighbours hold.
    graphics::Image pair(2, 2, math::Color::fromRgba8(255, 255, 255, 0));
    pair.setPixel(0, 0, math::Color::fromHex(0xFF0000FFU));
    const graphics::Image half = pair.halved();
    EXPECT_EQ(half.getWidth(), 1);
    EXPECT_EQ(half.getPixels()[0], 255);
    EXPECT_EQ(half.getPixels()[1], 0);
    EXPECT_EQ(half.getPixels()[3], 64);

    graphics::Image square(16, 4);
    square.buildMipmaps();
    ASSERT_EQ(square.getMipmaps().size(), 4U);
    EXPECT_EQ(square.getMipmaps()[1].getWidth(), 4);
    EXPECT_EQ(square.getMipmaps()[3].getWidth(), 1);
    EXPECT_EQ(square.getMipmaps()[3].getHeight(), 1);
    square.setPixel(0, 0, math::Color::white());
    EXPECT_TRUE(square.getMipmaps().empty());

    // Decoding for a linear texture bleeds, and decoding for mipmaps makes them.
    const std::vector<std::uint8_t> png = test::TestFiles::pngImage(4, 4, 0xFF0000FFU);
    EXPECT_EQ(graphics::Image::decodeFor(png, {.mipmaps = true}).getMipmaps().size(), 2U);
}

TEST(DeviceTest, MakesTexturesWithMipmaps) {
    test::EngineFixture fixture;
    graphics::Device& device = fixture.engine().getGraphics();
    const graphics::Texture made = device.createTexture(graphics::Image(32, 8), {.filter = graphics::Texture::Filter::Linear, .mipmaps = true});
    EXPECT_TRUE(made.getOptions().mipmaps);
    graphics::Image decoded(16, 16);
    decoded.buildMipmaps();
    EXPECT_EQ(device.createTexture(decoded, {.mipmaps = true}).getWidth(), 16);
    device.replaceTexture(made, graphics::Image(4, 4));
    EXPECT_EQ(made.getWidth(), 4);
    EXPECT_EQ(device.createTexture(graphics::Image(1, 1), {.mipmaps = true}).getWidth(), 1);
    EXPECT_THROW((void)device.createDynamicTexture(graphics::Image(4, 4), {.mipmaps = true}), std::invalid_argument);
    EXPECT_THROW((void)device.createRenderTarget(4, 4, {.mipmaps = true}), std::invalid_argument);
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

    EXPECT_THROW((void)device.createTexture(graphics::Image(0, 0)), std::invalid_argument);
    EXPECT_THROW((void)device.createRenderTarget(1, 1 << 20), std::invalid_argument);

    // A texture of one color checks its size before its pixels exist.
    EXPECT_EQ(device.createTexture(3, 2, math::Color::white()).getSize(), math::Vec2(3.0F, 2.0F));
    EXPECT_THROW((void)device.createTexture(1 << 20, 1 << 20, math::Color::white()), std::invalid_argument);
    EXPECT_THROW((void)device.createTexture(0, 2, math::Color::white()), std::invalid_argument);

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

// Dynamic textures change in place and reach the GPU once per frame, with the last pixels they received however many updates the frame made.
TEST(DeviceTest, UploadsEachChangedDynamicTextureOncePerFrame) {
    test::EngineFixture fixture;
    graphics::Device& device = fixture.engine().getGraphics();

    // The first frame sends the atlases of the default font and of the UI.
    fixture.frames(1);
    const graphics::Texture atlas = device.createDynamicAlphaTexture(4, 4, std::vector<std::uint8_t>(16, 0), {.filter = graphics::Texture::Filter::Linear});
    const graphics::Texture image = device.createDynamicTexture(2, 2, math::Color::white());
    const std::uint32_t atlasImage = atlas.getResource()->image.id;
    EXPECT_EQ(atlas.getOptions().filter, graphics::Texture::Filter::Linear);
    EXPECT_EQ(image.getSize(), math::Vec2(2.0F, 2.0F));

    // The first pixels wait for the next frame like any later change.
    fixture.frames(1);
    EXPECT_EQ(sg_query_stats().prev_frame.num_update_image, 2U);
    EXPECT_EQ(sg_query_stats().prev_frame.size_update_image, 16U + 16U);

    for (std::uint8_t glyph = 1; glyph <= 12; ++glyph) {
        device.updateTexture(atlas, std::vector<std::uint8_t>(16, glyph));
    }
    fixture.frames(1);
    EXPECT_EQ(sg_query_stats().prev_frame.num_update_image, 1U);
    EXPECT_EQ(sg_query_stats().prev_frame.size_update_image, 16U);
    EXPECT_EQ(fixture.engine().getRenderer2D().getStats().uploadedBytes, 16U);
    EXPECT_EQ(atlas.getResource()->image.id, atlasImage);

    fixture.frames(1);
    EXPECT_EQ(sg_query_stats().prev_frame.num_update_image, 0U);

    // A dynamic texture that takes a new size keeps its handle and stays dynamic, and its new pixels wait for the next frame.
    device.replaceTexture(image, graphics::Image(3, 1, math::Color::white()));
    EXPECT_EQ(image.getSize(), math::Vec2(3.0F, 1.0F));
    EXPECT_EQ(image.getResource()->staged.size(), 12U);
    device.updateTexture(image, std::vector<std::uint8_t>(12, 7));
    fixture.frames(1);
    EXPECT_EQ(sg_query_stats().prev_frame.num_update_image, 1U);
    EXPECT_EQ(sg_query_stats().prev_frame.size_update_image, 12U);

    // A texture released before the upload is skipped.
    graphics::Texture temporary = device.createDynamicTexture(graphics::Image(1, 1, math::Color::white()));
    temporary = {};
    EXPECT_EQ(device.uploadTextures(), 0U);

    EXPECT_THROW(device.updateTexture(atlas, std::vector<std::uint8_t>(3)), std::invalid_argument);
    EXPECT_THROW(device.updateTexture(device.createTexture(2, 2, math::Color::white()), std::vector<std::uint8_t>(16)), std::logic_error);
    EXPECT_THROW((void)device.createDynamicAlphaTexture(2, 2, std::vector<std::uint8_t>(3)), std::invalid_argument);
    EXPECT_THROW((void)device.createDynamicTexture(1 << 20, 1 << 20, math::Color::white()), std::invalid_argument);
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

// A dynamic texture that changes every frame, such as a video, keeps its staging memory, so frames that update it allocate no more than frames that do not, and a texture that stops changing gives the memory back.
TEST(DeviceTest, UpdatesDynamicTexturesEveryFrameWithoutAllocating) {
    test::EngineFixture fixture;
    graphics::Device& device = fixture.engine().getGraphics();
    const graphics::Texture texture = device.createDynamicTexture(64, 64, math::Color::white());
    const std::vector<std::uint8_t> pixels(std::size_t{64} * 64 * 4, 128);
    // clang-format off
    const auto measure = [&](bool update) {
        const test::AllocationTracker tracker;
        for (int frame = 0; frame < 3; ++frame) {
            if (update) {
                device.updateTexture(texture, pixels);
            }
            fixture.frames(1);
        }
        return tracker.getTotal();
    };
    // clang-format on
    for (int frame = 0; frame < 3; ++frame) {
        device.updateTexture(texture, pixels);
        fixture.frames(1);
    }

    const std::size_t updated = measure(true);
    EXPECT_EQ(fixture.engine().getRenderer2D().getStats().uploadedBytes, pixels.size());
    EXPECT_EQ(updated, measure(false));
    EXPECT_EQ(texture.getResource()->staged.capacity(), 0U);
}

TEST(DeviceTest, ReportsAFullPipelinePool) {
    test::EngineFixture fixture;
    sg_shader_desc shaderDesc{};
    shaderDesc.label = "empty";
    const sg_shader shader = graphics::Gpu::makeShader(shaderDesc);
    sg_pipeline_desc pipelineDesc{};
    pipelineDesc.shader = shader;
    pipelineDesc.label = "empty";

    std::vector<sg_pipeline> pipelines;
    std::string error;
    try {
        while (pipelines.size() <= static_cast<std::size_t>(graphics::Gpu::kPipelinePoolSize)) {
            pipelines.push_back(graphics::Gpu::makePipeline(pipelineDesc));
        }
    } catch (const std::runtime_error& failure) {
        error = failure.what();
    }
    EXPECT_EQ(error, "The graphics device has no room for another pipeline. At most 2048 pipelines can exist at once.");
    EXPECT_LE(pipelines.size(), static_cast<std::size_t>(graphics::Gpu::kPipelinePoolSize));

    for (const sg_pipeline pipeline : pipelines) {
        sg_destroy_pipeline(pipeline);
    }
    sg_destroy_shader(shader);
}

TEST(ViewportTest, FitLetterboxesAndConvertsCoordinates) {
    graphics::Viewport viewport;
    viewport.update(math::Vec2(1920.0F, 1200.0F), math::Vec2(1920.0F, 1080.0F), graphics::Viewport::ScalingPolicy::Fit);
    EXPECT_EQ(viewport.getPixelRect(), (math::Rect{0.0F, 60.0F, 1920.0F, 1080.0F}));
    EXPECT_EQ(viewport.getVisibleRect(), (math::Rect{0.0F, 0.0F, 1920.0F, 1080.0F}));
    EXPECT_EQ(viewport.toDesign(math::Vec2(960.0F, 600.0F)), math::Vec2(960.0F, 540.0F));
    EXPECT_EQ(viewport.toFramebuffer(math::Vec2(0.0F, 0.0F)), math::Vec2(0.0F, 60.0F));
    EXPECT_EQ(viewport.getScaling(), graphics::Viewport::ScalingPolicy::Fit);
    EXPECT_EQ(viewport.getDesignSize(), math::Vec2(1920.0F, 1080.0F));
    EXPECT_EQ(viewport.getFramebufferSize(), math::Vec2(1920.0F, 1200.0F));
}

TEST(ViewportTest, ExpandFillsTheScreenAroundTheDesignArea) {
    graphics::Viewport viewport;
    viewport.update(math::Vec2(2340.0F, 1080.0F), math::Vec2(1920.0F, 1080.0F), graphics::Viewport::ScalingPolicy::Expand, math::Insets{90.0F, 0.0F, 90.0F, 40.0F});
    EXPECT_EQ(viewport.getPixelRect(), (math::Rect{0.0F, 0.0F, 2340.0F, 1080.0F}));
    EXPECT_EQ(viewport.getVisibleRect(), (math::Rect{-210.0F, 0.0F, 2340.0F, 1080.0F}));
    EXPECT_EQ(viewport.getSafeRect(), (math::Rect{-120.0F, 0.0F, 2160.0F, 1040.0F}));
    EXPECT_EQ(viewport.getPixelsPerUnit(), math::Vec2(1.0F, 1.0F));
}

TEST(ViewportTest, FillStretchAndPixelPerfect) {
    graphics::Viewport viewport;
    viewport.update(math::Vec2(1000.0F, 1000.0F), math::Vec2(2000.0F, 1000.0F), graphics::Viewport::ScalingPolicy::Fill);
    EXPECT_EQ(viewport.getVisibleRect(), (math::Rect{500.0F, 0.0F, 1000.0F, 1000.0F}));

    viewport.update(math::Vec2(1000.0F, 1000.0F), math::Vec2(2000.0F, 1000.0F), graphics::Viewport::ScalingPolicy::Stretch);
    EXPECT_EQ(viewport.getPixelsPerUnit(), math::Vec2(0.5F, 1.0F));

    viewport.update(math::Vec2(1000.0F, 700.0F), math::Vec2(320.0F, 180.0F), graphics::Viewport::ScalingPolicy::PixelPerfect);
    EXPECT_EQ(viewport.getPixelRect(), (math::Rect{20.0F, 80.0F, 960.0F, 540.0F}));

    viewport.update(math::Vec2(0.0F, 0.0F), math::Vec2(0.0F, 0.0F), graphics::Viewport::ScalingPolicy::Fit);
    EXPECT_EQ(viewport.getFramebufferSize(), math::Vec2(1.0F, 1.0F));

    EXPECT_EQ(graphics::Viewport::scalingPolicyFromName("expand"), graphics::Viewport::ScalingPolicy::Expand);
    EXPECT_FALSE(graphics::Viewport::scalingPolicyFromName("zoom").has_value());
}

TEST(ViewportTest, FitsTheWidthOrScalesNothing) {
    // A tall screen shows the whole design width and more height above and below the design area.
    graphics::Viewport viewport;
    viewport.update(math::Vec2(1080.0F, 2400.0F), math::Vec2(1920.0F, 1080.0F), graphics::Viewport::ScalingPolicy::FitWidth);
    EXPECT_EQ(viewport.getPixelRect(), (math::Rect{0.0F, 0.0F, 1080.0F, 2400.0F}));
    const math::Rect visible = viewport.getVisibleRect();
    EXPECT_EQ(visible.x, 0.0F);
    EXPECT_EQ(visible.width, 1920.0F);
    EXPECT_NEAR(visible.height, 4266.67F, 0.01F);
    EXPECT_NEAR(visible.getCenter().y, 540.0F, 0.01F);

    // Without scaling a design unit is a point, so the visible area follows the window from the top left.
    viewport.update(math::Vec2(2560.0F, 1600.0F), math::Vec2(1920.0F, 1080.0F), graphics::Viewport::ScalingPolicy::None, {}, 2.0F);
    EXPECT_EQ(viewport.getVisibleRect(), (math::Rect{0.0F, 0.0F, 1280.0F, 800.0F}));
    EXPECT_EQ(viewport.getPixelsPerUnit(), math::Vec2(2.0F, 2.0F));
    EXPECT_EQ(graphics::Viewport::scalingPolicyName(graphics::Viewport::ScalingPolicy::FitWidth), "fitWidth");
}

TEST(ViewportTest, PixelPerfectShrinksByWholeDivisorsOnSmallFramebuffers) {
    graphics::Viewport viewport;
    viewport.update(math::Vec2(800.0F, 600.0F), math::Vec2(1920.0F, 1080.0F), graphics::Viewport::ScalingPolicy::PixelPerfect);
    EXPECT_EQ(viewport.getPixelRect(), (math::Rect{80.0F, 120.0F, 640.0F, 360.0F}));
    EXPECT_EQ(viewport.getVisibleRect(), (math::Rect{0.0F, 0.0F, 1920.0F, 1080.0F}));
    EXPECT_EQ(viewport.getPixelsPerUnit(), math::Vec2(1.0F / 3.0F, 1.0F / 3.0F));

    viewport.update(math::Vec2(960.0F, 540.0F), math::Vec2(1920.0F, 1080.0F), graphics::Viewport::ScalingPolicy::PixelPerfect);
    EXPECT_EQ(viewport.getPixelRect(), (math::Rect{0.0F, 0.0F, 960.0F, 540.0F}));
}

// A centered viewport lands on whole pixels, where the GPU places it, so pixel art never starts half a pixel in and input maps to the pixels the screen shows.
TEST(ViewportTest, LandsOnWholePixels) {
    graphics::Viewport viewport;
    viewport.update(math::Vec2(1001.0F, 601.0F), math::Vec2(500.0F, 300.0F), graphics::Viewport::ScalingPolicy::PixelPerfect);
    EXPECT_EQ(viewport.getPixelRect(), (math::Rect{1.0F, 1.0F, 1000.0F, 600.0F}));
    EXPECT_EQ(viewport.getPixelsPerUnit(), math::Vec2(2.0F, 2.0F));
    EXPECT_EQ(viewport.toFramebuffer(math::Vec2(10.0F, 20.0F)), math::Vec2(21.0F, 41.0F));

    viewport.update(math::Vec2(1366.0F, 769.0F), math::Vec2(1920.0F, 1080.0F), graphics::Viewport::ScalingPolicy::Fit);
    const math::Rect fitted = viewport.getPixelRect();
    for (const float edge : {fitted.x, fitted.y, fitted.getRight(), fitted.getBottom()}) {
        EXPECT_EQ(edge, std::round(edge));
    }
    EXPECT_EQ(viewport.toDesign(fitted.getMin()), math::Vec2(0.0F, 0.0F));

    // A design that fits into less than a pixel still takes one, so every conversion stays finite.
    viewport.update(math::Vec2(1.0F, 2.0F), math::Vec2(1920.0F, 1080.0F), graphics::Viewport::ScalingPolicy::Fit);
    EXPECT_EQ(viewport.getPixelRect(), (math::Rect{0.0F, 1.0F, 1.0F, 1.0F}));
    EXPECT_TRUE(std::isfinite(viewport.toDesign(math::Vec2(1.0F, 1.0F)).y));
    EXPECT_TRUE(std::isfinite(viewport.getSafeRect().height));
}

// Each enum keeps its names in one place, which Lua, asset options and `app.json` all read, and every value reads back as the name it came from.
TEST(GraphicsNamesTest, ParsesAndNamesBlendFilterWrapAndScalingValues) {
    EXPECT_EQ(graphics::BlendMode::parse("additive"), graphics::BlendMode::Type::Additive);
    EXPECT_EQ(graphics::BlendMode::name(graphics::BlendMode::Type::Screen), "screen");
    EXPECT_FALSE(graphics::BlendMode::parse("burn").has_value());
    EXPECT_FALSE(graphics::Texture::filterFromName("cubic").has_value());
    EXPECT_FALSE(graphics::Texture::wrapFromName("border").has_value());
    EXPECT_FALSE(graphics::Viewport::scalingPolicyFromName("zoom").has_value());

    for (const std::string_view name : {"nearest", "linear"}) {
        EXPECT_EQ(graphics::Texture::filterName(*graphics::Texture::filterFromName(name)), name);
    }
    for (const std::string_view name : {"clamp", "repeat", "mirror"}) {
        EXPECT_EQ(graphics::Texture::wrapName(*graphics::Texture::wrapFromName(name)), name);
    }
    for (const std::string_view name : {"fit", "fill", "stretch", "expand", "pixelPerfect"}) {
        EXPECT_EQ(graphics::Viewport::scalingPolicyName(*graphics::Viewport::scalingPolicyFromName(name)), name);
    }
    EXPECT_EQ(graphics::Texture::filterFromName("linear"), graphics::Texture::Filter::Linear);
    EXPECT_EQ(graphics::Texture::wrapFromName("mirror"), graphics::Texture::Wrap::Mirror);
    EXPECT_EQ(graphics::Viewport::scalingPolicyFromName("pixelPerfect"), graphics::Viewport::ScalingPolicy::PixelPerfect);
}

} // namespace haylen
