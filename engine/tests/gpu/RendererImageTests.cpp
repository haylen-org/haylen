#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "gpu/DeviceFactory.hpp"
#include "gpu/support/GpuFixture.hpp"
#include "gpu/support/ImageComparison.hpp"
#include "gpu/support/LoggedErrors.hpp"
#include "gpu/support/OffscreenSurface.hpp"
#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/graphics/PostProcess.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/graphics/SpriteData.hpp"
#include "haylen/2d/graphics/SpriteInstance.hpp"
#include "haylen/2d/lighting/Light.hpp"
#include "haylen/2d/lighting/Occluder.hpp"
#include "haylen/2d/particles/Emitter.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/RenderTarget.hpp"
#include "support/DrawingScene.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::gpu {

// Draws reference scenes with the 2D renderer of a real engine on the device of each graphics API, offscreen into a render target, and compares them with the reference images of `engine/tests/data/gpu`, which hold for every API.
class GpuRendererTest : public ::testing::TestWithParam<Device::Api> {
  protected:
    static constexpr int kSize = 128;

    // The difference in 255ths that a channel may take, and the pixels that may pass it, which leave room for the edges that APIs rasterize apart.
    static constexpr int kTolerance = 6;
    static constexpr int kAllowedPixels = kSize * kSize / 100;

    void SetUp() override {
        if (GetParam() == Device::Api::None) {
            FAIL() << "The variable \"HAYLEN_TEST_GRAPHICS_API\" names no graphics API.";
        }
        errors = std::make_unique<test::LoggedErrors>();

        // clang-format off
        const auto makeDevice = [this]() -> std::unique_ptr<Device> {
            return test::GpuFixture::makeDevice(GetParam(), surface);
        };
        // clang-format on
        try {
            fixture = std::make_unique<test::EngineFixture>(std::map<std::string, std::string>{}, nullptr, test::EngineFixture::Options{.gpuDevice = makeDevice});
        } catch (const std::runtime_error& error) {
            if (test::GpuFixture::isApiRequired()) {
                FAIL() << "The device of \"" << DeviceFactory::getApiName(GetParam()) << "\", which \"HAYLEN_TEST_GRAPHICS_API\" names, cannot start on this machine: " << error.what();
            }
            GTEST_SKIP() << "The device of \"" << DeviceFactory::getApiName(GetParam()) << "\" cannot start on this machine: " << error.what();
        }
        target = fixture->engine().getGraphics().createRenderTarget(kSize, kSize);
    }

    // The engine and its device go before the surface they draw into.
    void TearDown() override {
        target = {};
        fixture.reset();
        surface.reset();
        if (errors && !IsSkipped()) {
            errors->reportAsFailures();
        }
    }

    using Draw = std::function<void(core::Engine&, graphics2d::Renderer&, const graphics::RenderTarget&)>;

    // Draws the scene into the render target in one frame of the engine, reads the target back and compares it with the reference of the name.
    void expectScene(std::string_view name, Draw draw) {
        std::optional<graphics::Image> image;
        readScene(name, std::move(draw), image);
        if (image) {
            test::ImageComparison::expectReference(*image, name, kTolerance, kAllowedPixels);
        }
    }

    // Draws the scene into the render target in one frame of the engine and reads the target back into the image.
    void readScene(std::string_view name, Draw draw, std::optional<graphics::Image>& image) {
        // clang-format off
        fixture->engine().getScenes().replace(std::make_shared<test::DrawingScene>([this, draw = std::move(draw)](core::Engine& engine) {
            draw(engine, engine.getRenderer2D(), target);
        }));
        // clang-format on
        fixture->frames(2);

        std::string failure;
        bool done = false;
        // clang-format off
        fixture->engine().getGraphics().readTarget(target, [&](std::optional<graphics::Image> read, const std::string& error) {
            image = std::move(read);
            failure = error;
            done = true;
        });
        // clang-format on
        ASSERT_TRUE(fixture->frameUntil([&done] { return done; })) << "The read of the scene \"" << name << "\" never finished.";
        ASSERT_TRUE(image.has_value()) << failure;
    }

    // The center of the cell of a sprite in a grid of four columns and two rows that fills the target.
    [[nodiscard]] static math::Vec2 cellCenter(std::size_t index) {
        return {static_cast<float>(index % 4) * 32.0F + 16.0F, static_cast<float>(index / 4) * 64.0F + 32.0F};
    }

    // A camera that shows the target with the world origin at its top-left corner.
    [[nodiscard]] static graphics2d::Camera topLeft() {
        graphics2d::Camera camera;
        camera.anchor = graphics2d::Camera::Anchor::TopLeft;
        return camera;
    }

    std::unique_ptr<test::LoggedErrors> errors;
    std::unique_ptr<test::OffscreenSurface> surface;
    std::unique_ptr<test::EngineFixture> fixture;
    graphics::RenderTarget target;
};

// Rectangles, circles, rings, arcs, ellipses, polygons and lines, with the edges of one pixel that every curve of the engine fades over.
TEST_P(GpuRendererTest, DrawsShapes) {
    // clang-format off
    expectScene("shapes", [](core::Engine&, graphics2d::Renderer& renderer, const graphics::RenderTarget& into) {
        renderer.beginTarget(into, topLeft(), {.clear = math::Color::fromHex(0x1B2A3AFFU)});
        renderer.drawRect({8.0F, 8.0F, 40.0F, 24.0F}, math::Color::fromHex(0xE8A33DFFU));
        renderer.drawRectOutline({56.0F, 8.0F, 40.0F, 24.0F}, 3.0F, math::Color::fromHex(0x7BC67BFFU));
        renderer.drawCircle({28.0F, 60.0F}, 18.0F, math::Color::fromHex(0x4FA3E0FFU));
        renderer.drawRing({76.0F, 60.0F}, 16.0F, 4.0F, math::Color::fromHex(0xF2F2F2FFU));
        renderer.drawArc({108.0F, 60.0F}, 14.0F, 5.0F, 0.0F, std::numbers::pi_v<float>, math::Color::fromHex(0xD9534FFFU));
        renderer.drawEllipse({30.0F, 104.0F}, {22.0F, 12.0F}, math::Color::fromHex(0xB07CC6FFU));
        const std::array<math::Vec2, 5> star{{{76.0F, 84.0F}, {92.0F, 120.0F}, {58.0F, 98.0F}, {94.0F, 98.0F}, {60.0F, 120.0F}}};
        renderer.drawPolygon(star, math::Color::fromHex(0xF5D547CCU));
        renderer.drawLine({100.0F, 86.0F}, {124.0F, 122.0F}, 2.0F, math::Color::fromHex(0x5BC0BEFFU));
    });
    // clang-format on
}

// Sprites of a generated texture, tinted, turned, flipped, scaled with both filters and blended in the modes of the engine.
TEST_P(GpuRendererTest, DrawsSprites) {
    graphics::Image checker(8, 8, math::Color::white());
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            checker.setPixel(x, y, (x + y) % 2 == 0 ? math::Color::fromHex(0xFF6B6BFFU) : math::Color::fromHex(0x4ECDC4FFU));
        }
    }
    graphics::Device& graphics = fixture->engine().getGraphics();
    const graphics::Texture nearest = graphics.createTexture(checker, {.filter = graphics::Texture::Filter::Nearest});
    const graphics::Texture linear = graphics.createTexture(checker, {.filter = graphics::Texture::Filter::Linear});

    // clang-format off
    expectScene("sprites", [&](core::Engine&, graphics2d::Renderer& renderer, const graphics::RenderTarget& into) {
        renderer.beginTarget(into, topLeft(), {.clear = math::Color::fromHex(0x202020FFU)});
        renderer.draw({.texture = nearest, .position = {32.0F, 32.0F}, .size = {48.0F, 48.0F}});
        renderer.draw({.texture = linear, .position = {96.0F, 32.0F}, .size = {48.0F, 48.0F}, .rotation = std::numbers::pi_v<float> / 6.0F});
        renderer.draw({.texture = nearest, .position = {32.0F, 96.0F}, .size = {40.0F, 40.0F}, .color = math::Color::fromHex(0x80C0FFFFU), .flip = {.horizontal = true}});
        renderer.draw({.texture = nearest, .position = {88.0F, 96.0F}, .size = {40.0F, 40.0F}, .color = math::Color{1.0F, 1.0F, 1.0F, 0.6F}});
        renderer.draw({.texture = nearest, .position = {104.0F, 104.0F}, .size = {40.0F, 40.0F}, .order = {.layer = 1, .blend = graphics::BlendMode::Type::Additive}});
    });
    // clang-format on
}

// A batch of eight textures in one draw call, whose last texture takes slot 31. The Metal device binds that slot at a compact index, since the Apple TV 4K of the first generation binds only the texture indices 0 to 30.
TEST_P(GpuRendererTest, DrawsBatchesThatReachEveryTextureOfTheBatch) {
    const std::array<math::Color, graphics2d::SpriteData::kMaxTextures> colors{math::Color::fromHex(0xFF0000FFU), math::Color::fromHex(0x00FF00FFU), math::Color::fromHex(0x0000FFFFU), math::Color::fromHex(0xFFFF00FFU), math::Color::fromHex(0xFF00FFFFU), math::Color::fromHex(0x00FFFFFFU), math::Color::fromHex(0xFFFFFFFFU), math::Color::fromHex(0xFF8000FFU)};
    std::vector<graphics::Texture> textures;
    std::vector<graphics2d::SpriteInstance> sprites;
    std::vector<graphics2d::SpriteData> data;
    for (std::size_t index = 0; index < colors.size(); ++index) {
        textures.push_back(fixture->engine().getGraphics().createTexture(graphics::Image(2, 2, colors[index]), {.filter = graphics::Texture::Filter::Nearest}));
        sprites.push_back({.position = cellCenter(index), .size = {24.0F, 24.0F}});
        data.push_back({.texture = static_cast<std::uint8_t>(index)});
    }

    std::optional<graphics::Image> image;
    // clang-format off
    readScene("batch", [&](core::Engine&, graphics2d::Renderer& renderer, const graphics::RenderTarget& into) {
        renderer.beginTarget(into, topLeft(), {.clear = math::Color::black()});
        renderer.drawBatch(textures, sprites, data);
    }, image);
    // clang-format on
    ASSERT_TRUE(image.has_value());

    for (std::size_t index = 0; index < colors.size(); ++index) {
        const math::Vec2 center = cellCenter(index);
        EXPECT_EQ(image->getPixel(static_cast<int>(center.x), static_cast<int>(center.y)).toHex(), colors[index].toHex()) << "The sprite of the texture " << index << " drew the wrong color.";
    }
}

// Text of the font that the engine embeds, with an outline and a shadow.
TEST_P(GpuRendererTest, DrawsText) {
    // clang-format off
    expectScene("text", [](core::Engine& engine, graphics2d::Renderer& renderer, const graphics::RenderTarget& into) {
        renderer.beginTarget(into, topLeft(), {.clear = math::Color::fromHex(0x101820FFU)});
        renderer.drawText(*engine.getDefaultFont(), "Haylen", {8.0F, 8.0F}, {.size = 28.0F, .color = math::Color::fromHex(0xF2AA4CFFU)});
        renderer.drawText(*engine.getDefaultFont(), "Every API\ndraws alike", {8.0F, 52.0F}, {.size = 16.0F, .outlineWidth = 1.5F, .outlineColor = math::Color::black(), .shadowOffset = {2.0F, 2.0F}, .shadowColor = math::Color{0.0F, 0.0F, 0.0F, 0.5F}});
    });
    // clang-format on
}

// A lit canvas with an ambient light, a point light whose occluder casts a shadow, and a colored spot light.
TEST_P(GpuRendererTest, DrawsLightsAndShadows) {
    // clang-format off
    expectScene("lighting", [](core::Engine&, graphics2d::Renderer& renderer, const graphics::RenderTarget& into) {
        renderer.beginTarget(into, topLeft(), {.ambientLight = math::Color{0.15F, 0.15F, 0.2F, 1.0F}, .clear = math::Color::black()});
        renderer.drawRect({0.0F, 0.0F, static_cast<float>(kSize), static_cast<float>(kSize)}, math::Color::fromHex(0xC8C0B0FFU));
        renderer.drawRect({56.0F, 56.0F, 16.0F, 16.0F}, math::Color::fromHex(0x6B4F3AFFU));
        renderer.drawOccluder({.points = {{56.0F, 56.0F}, {72.0F, 56.0F}, {72.0F, 72.0F}, {56.0F, 72.0F}}});
        renderer.drawLight({.position = {32.0F, 40.0F}, .radius = 96.0F, .color = math::Color::fromHex(0xFFE2A8FFU), .intensity = 1.2F, .shadows = true});
        renderer.drawLight({.type = lighting2d::Light::Type::Spot, .position = {112.0F, 112.0F}, .radius = 72.0F, .color = math::Color::fromHex(0x6FA8FFFFU), .rotation = -2.4F, .innerAngle = 0.3F, .outerAngle = 0.6F});
    });
    // clang-format on
}

// Particles of an emitter with a fixed seed, after a second of simulation, drawn additively.
TEST_P(GpuRendererTest, DrawsParticles) {
    const graphics::Texture dot = fixture->engine().getGraphics().createTexture(graphics::Image(4, 4, math::Color::white()));
    particles2d::Emitter emitter({.texture = dot, .rate = 60.0F, .maxParticles = 64, .lifetime = {1.5F, 1.5F}, .speed = {40.0F, 60.0F}, .direction = -std::numbers::pi_v<float> / 2.0F, .spread = 0.8F, .gravity = {0.0F, 30.0F}, .startSize = {6.0F, 6.0F}, .endSize = {2.0F, 2.0F}, .colors = {math::Color::fromHex(0xFFD166FFU), math::Color::fromHex(0xEF476F80U)}, .order = {.blend = graphics::BlendMode::Type::Additive}}, 7);
    emitter.position = {64.0F, 112.0F};
    for (int step = 0; step < 60; ++step) {
        emitter.update(1.0F / 60.0F);
    }

    // clang-format off
    expectScene("particles", [&emitter](core::Engine&, graphics2d::Renderer& renderer, const graphics::RenderTarget& into) {
        renderer.beginTarget(into, topLeft(), {.clear = math::Color::fromHex(0x0B0F1AFFU)});
        emitter.draw(renderer);
    });
    // clang-format on
}

// Post-processing of a target canvas: saturation, tint, vignette and contrast over shapes of every color.
TEST_P(GpuRendererTest, PostProcessesCanvases) {
    // clang-format off
    expectScene("post-process", [](core::Engine&, graphics2d::Renderer& renderer, const graphics::RenderTarget& into) {
        renderer.beginTarget(into, topLeft(), {.postProcess = graphics2d::PostProcess{.tint = math::Color::fromHex(0xFFF0D8FFU), .saturation = 0.6F, .contrast = 1.2F, .vignetteStrength = 0.6F}, .clear = math::Color::fromHex(0x3A86FFFFU)});
        for (int index = 0; index < 4; ++index) {
            const float x = 16.0F + static_cast<float>(index) * 28.0F;
            renderer.drawCircle({x + 10.0F, 64.0F}, 12.0F, math::Color::fromHsv(static_cast<float>(index) / 4.0F, 0.8F, 1.0F));
        }
    });
    // clang-format on
}

// A render target that a first canvas fills and a second canvas draws as a sprite, turned and scaled, the way the engine composes its offscreen images.
TEST_P(GpuRendererTest, ComposesRenderTargets) {
    const graphics::RenderTarget inner = fixture->engine().getGraphics().createRenderTarget(32, 32, {.filter = graphics::Texture::Filter::Linear});
    // clang-format off
    expectScene("render-targets", [&inner](core::Engine&, graphics2d::Renderer& renderer, const graphics::RenderTarget& into) {
        renderer.beginTarget(inner, topLeft(), {.clear = math::Color::fromHex(0x2EC4B6FFU)});
        renderer.drawCircle({16.0F, 16.0F}, 10.0F, math::Color::fromHex(0xFF9F1CFFU));
        renderer.drawRect({0.0F, 0.0F, 8.0F, 8.0F}, math::Color::white());

        renderer.beginTarget(into, topLeft(), {.clear = math::Color::fromHex(0x011627FFU)});
        renderer.draw({.texture = inner.getTexture(), .position = {36.0F, 36.0F}, .size = {48.0F, 48.0F}});
        renderer.draw({.texture = inner.getTexture(), .position = {92.0F, 92.0F}, .size = {48.0F, 48.0F}, .rotation = std::numbers::pi_v<float> / 4.0F});
    });
    // clang-format on
}

// Groups whose blends mix with what lies below, which read a copy of what their canvas drew before them, over stripes of four colors.
TEST_P(GpuRendererTest, BlendsGroupsWithWhatLiesBelow) {
    // clang-format off
    expectScene("blend-groups", [](core::Engine&, graphics2d::Renderer& renderer, const graphics::RenderTarget& into) {
        renderer.beginTarget(into, topLeft(), {.clear = math::Color::fromHex(0x1B2A41FFU)});
        const std::array<std::uint32_t, 4> stripes{0xE63946FFU, 0xF1FAEEFFU, 0x457B9DFFU, 0xFFB703FFU};
        for (std::size_t index = 0; index < stripes.size(); ++index) {
            renderer.drawRect({static_cast<float>(index) * 32.0F, 0.0F, 32.0F, static_cast<float>(kSize)}, math::Color::fromHex(stripes[index]));
        }
        const std::array<std::pair<graphics::BlendMode::Type, float>, 3> groups{{{graphics::BlendMode::Type::Overlay, 32.0F}, {graphics::BlendMode::Type::Difference, 64.0F}, {graphics::BlendMode::Type::ColorBurn, 96.0F}}};
        for (const auto& [blend, y] : groups) {
            renderer.beginGroup({.opacity = 0.9F, .order = {.blend = blend}});
            renderer.drawCircle({48.0F, y}, 22.0F, math::Color::fromHex(0x2A9D8FFFU));
            renderer.drawCircle({80.0F, y}, 22.0F, math::Color::fromHex(0xE9C46AFFU));
            renderer.endGroup();
        }
    });
    // clang-format on
}

INSTANTIATE_TEST_SUITE_P(Devices, GpuRendererTest, ::testing::ValuesIn(test::GpuFixture::getTestedApis()), test::GpuFixture::nameApi);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GpuRendererTest);

} // namespace haylen::gpu
