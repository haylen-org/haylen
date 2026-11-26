#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "2d/graphics/MaterialResource.hpp"
#include "graphics/DeviceState.hpp"
#include "graphics/ShaderResource.hpp"
#include "haylen/2d/graphics/Material.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/graphics/SpriteData.hpp"
#include "haylen/2d/graphics/SpriteInstance.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/graphics/Shader.hpp"
#include "support/AllocationTracker.hpp"
#include "support/DrawingScene.hpp"
#include "support/EngineFixture.hpp"
#include "support/MaterialShaders.hpp"
#include "support/TestFiles.hpp"

namespace haylen::graphics2d {

class MaterialTest : public ::testing::Test {
  protected:
    [[nodiscard]] static float floatAt(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
        float value = 0.0F;
        std::memcpy(&value, bytes.data() + offset, sizeof(value));
        return value;
    }
};

TEST_F(MaterialTest, SetsUniformsByNameAndPacksThemInTheirBlocks) {
    graphics2d::Material material(graphics::Shader::parse(test::MaterialShaders::tint()));
    EXPECT_TRUE(material.isValid());
    EXPECT_EQ(material.getShader().getName(), "tint");
    EXPECT_EQ(material.get("strength"), std::vector<float>{0.0F});

    material.set("tint", math::Color{0.5F, 0.25F, 1.0F, 1.0F});
    material.set("strength", 0.75F);
    material.set("offset", math::Vec2{3.0F, 4.0F});
    material.set("steps", 2.0F);
    material.set("transform", math::Transform2D::translation({10.0F, 20.0F}));
    material.set("glow", math::Color{0.1F, 0.2F, 0.3F, 1.0F});
    const std::vector<float> palette(12, 0.5F);
    material.set("palette", palette);
    material.set("weights", std::vector<float>{1.0F, 2.0F, 3.0F});
    const std::uint32_t revision = material.getResource()->revision;
    EXPECT_EQ(revision, 8U);

    EXPECT_EQ(material.get("offset"), (std::vector<float>{3.0F, 4.0F}));
    EXPECT_EQ(material.get("glow").size(), 3U);
    EXPECT_EQ(material.get("transform")[12], 10.0F);

    std::vector<std::uint8_t> bytes;
    material.getResource()->pack(bytes);
    ASSERT_EQ(bytes.size(), 144U + 64U + 16U);
    EXPECT_EQ(floatAt(bytes, 4), 0.25F);
    EXPECT_EQ(floatAt(bytes, 16), 0.75F);
    EXPECT_EQ(floatAt(bytes, 28), 4.0F);
    EXPECT_EQ(floatAt(bytes, 32 + 13 * 4), 20.0F);
    EXPECT_EQ(floatAt(bytes, 96 + 11 * 4), 0.5F);
    EXPECT_EQ(floatAt(bytes, 144 + 8), 0.3F);

    // Every element of an array of floats takes a whole vector of four in the std140 layout.
    EXPECT_EQ(floatAt(bytes, 144 + 16), 1.0F);
    EXPECT_EQ(floatAt(bytes, 144 + 32), 2.0F);
    EXPECT_EQ(floatAt(bytes, 144 + 48), 3.0F);
    EXPECT_EQ(floatAt(bytes, 144 + 20), 0.0F);
    std::int32_t steps = 0;
    std::memcpy(&steps, bytes.data() + 208, sizeof(steps));
    EXPECT_EQ(steps, 2);

    test::EngineFixture fixture;
    const graphics::Texture mask = fixture.engine().getGraphics().createTexture(graphics::Image(2, 2, math::Color::white()));
    material.setTexture("mask_texture", mask);
    EXPECT_EQ(material.getTexture("mask_texture"), mask);
    EXPECT_EQ(material.getResource()->getTextureAt(1), mask);
    EXPECT_FALSE(material.getResource()->getTextureAt(4).isValid());

    EXPECT_THROW(material.set("missing", 1.0F), std::invalid_argument);
    EXPECT_THROW(material.set("tint", 1.0F), std::invalid_argument);
    EXPECT_THROW(material.set("strength", math::Vec2{}), std::invalid_argument);
    EXPECT_THROW(material.set("palette", std::vector<float>(4, 0.0F)), std::invalid_argument);
    EXPECT_THROW(material.setTexture("missing", mask), std::invalid_argument);
    EXPECT_THROW((void)material.getTexture("tint"), std::invalid_argument);
    EXPECT_EQ(material.getResource()->revision, revision + 1);

    EXPECT_THROW(graphics2d::Material{graphics::Shader{}}, std::invalid_argument);
    EXPECT_THROW((void)graphics2d::Material{}.getShader(), std::logic_error);
    const std::string bare = R"({"format": "haylen-shader", "version": 4, "name": "bare", "sources": [], "programs": {"sprite": {"attributes": [], "blocks": [], "textures": [], "samplers": [], "pairs": [], "outputs": 1, "targets": {}}}})";
    EXPECT_THROW(graphics2d::Material{graphics::Shader::parse(test::TestFiles::bytes(bare))}, std::invalid_argument);
}

// A value an app sets every frame, such as a time, and the copy every frame packs of it reuse their storage.
TEST_F(MaterialTest, SetsAndPacksValuesEveryFrameWithoutAllocating) {
    graphics2d::Material material(graphics::Shader::parse(test::MaterialShaders::tint()));
    material.set("strength", 0.5F);
    material.set("palette", std::vector<float>(12, 0.5F));
    std::vector<std::uint8_t> bytes;
    material.getResource()->pack(bytes);

    const test::AllocationTracker tracker;

    for (int frame = 1; frame <= 10; ++frame) {
        material.set("strength", static_cast<float>(frame));
        bytes.clear();
        material.getResource()->pack(bytes);
    }

    EXPECT_EQ(tracker.getTotal(), 0U);
    EXPECT_EQ(floatAt(bytes, 16), 10.0F);
    EXPECT_EQ(floatAt(bytes, 96), 0.5F);
}

TEST_F(MaterialTest, DrawsEveryKindOfDrawAndPostProcessesCanvases) {
    const std::string compiled(test::MaterialShaders::tint().begin(), test::MaterialShaders::tint().end());
    test::EngineFixture fixture({{"content/shaders/tint.shader", compiled}});
    graphics::Device& device = fixture.engine().getGraphics();
    const graphics::Shader shader = fixture.engine().getAssets().shader("shaders/tint.shader");
    graphics2d::Material material(shader);
    material.set("strength", 1.0F);
    const graphics2d::Material post(shader);
    const graphics::Texture texture = device.createTexture(graphics::Image(8, 8, math::Color::white()));
    const std::vector<graphics::Texture> textures{texture, device.createTexture(graphics::Image(16, 16, math::Color::white()))};
    const std::vector<graphics2d::SpriteInstance> sprites(2, graphics2d::SpriteInstance{.size = {4.0F, 4.0F}});
    const std::vector<graphics2d::SpriteData> data{{.values = {1.0F}}, {.texture = 1, .values = {0.5F}}};
    const graphics::RenderTarget target = device.createRenderTarget(64, 64);

    // clang-format off
    fixture.engine().getScenes().replace(std::make_shared<test::DrawingScene>([&](core::Engine& engine) {
        graphics2d::Renderer& renderer = engine.getRenderer2D();
        const graphics2d::DrawOrder order{.material = material};
        for (const bool lit : {false, true}) {
            renderer.beginWorld(graphics2d::Camera{}, {.ambientLight = lit ? std::optional(math::Color::white()) : std::nullopt, .postProcess = graphics2d::PostProcess{.materials = {post, post}}});
            renderer.draw({.texture = texture, .order = order});
            renderer.drawBatch(textures, sprites, data, order);
            renderer.drawBatch(std::span(textures).first(1), sprites, std::span(data).first(1), order);
            material.set("strength", 0.5F);
            renderer.draw({.texture = texture, .order = order});
            renderer.drawRect({0.0F, 0.0F, 4.0F, 4.0F}, math::Color::white(), order);
            renderer.drawCircle({0.0F, 0.0F}, 8.0F, math::Color::white(), order);
            renderer.drawText(*engine.getDefaultFont(), "Hi", {}, {}, order);
        }

        renderer.beginTarget(target, graphics2d::Camera{}, {.ambientLight = math::Color::white(), .postProcess = graphics2d::PostProcess{.materials = {post}}});
        renderer.draw({.texture = texture, .order = order});
    }));
    // clang-format on
    fixture.frames(2);

    // Each canvas copies the material once per change of its values, so both draws of a canvas keep their own strength.
    const graphics2d::Renderer::Stats& stats = fixture.engine().getRenderer2D().getStats();
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    EXPECT_EQ(stats.canvases, 3U);
    EXPECT_GE(stats.drawCalls, 19U);
    EXPECT_EQ(material.get("strength"), std::vector<float>{0.5F});

    // Materials replace the sprite, batch, text, mesh and shape programs only.
    graphics2d::Renderer& renderer = fixture.engine().getRenderer2D();
    renderer.beginScreen();
    EXPECT_THROW(renderer.drawImageBlend({.from = texture, .to = texture}, {.material = material}), std::invalid_argument);
    EXPECT_THROW(renderer.drawMetaballs(std::vector<math::Vec2>{{}}, 4.0F, {}, {.material = material}), std::invalid_argument);
    fixture.frames(1);
}

TEST_F(MaterialTest, DrawsWithAShaderThatNeverSamplesItsTexture) {
    const std::vector<std::uint8_t> bytes = test::MaterialShaders::compile("glow", test::MaterialShaders::kGlowSource);
    test::EngineFixture fixture({{"content/shaders/glow.shader", std::string(bytes.begin(), bytes.end())}});
    graphics::Device& device = fixture.engine().getGraphics();
    const graphics2d::Material material(fixture.engine().getAssets().shader("shaders/glow.shader"));
    const graphics::Texture texture = device.createTexture(graphics::Image(8, 8, math::Color::white()));
    const std::array<math::Vec2, 3> points{math::Vec2{0.0F, 0.0F}, math::Vec2{20.0F, 10.0F}, math::Vec2{40.0F, 0.0F}};

    // Every program that a material replaces binds the sampler of the sprite texture, which the shader declares and never pairs.
    // clang-format off
    fixture.engine().getScenes().replace(std::make_shared<test::DrawingScene>([&](core::Engine& engine) {
        graphics2d::Renderer& renderer = engine.getRenderer2D();
        const graphics2d::DrawOrder order{.material = material};
        for (const bool lit : {false, true}) {
            renderer.beginWorld(graphics2d::Camera{}, {.ambientLight = lit ? std::optional(math::Color::white()) : std::nullopt});
            renderer.draw({.texture = texture, .order = order});
            renderer.drawStroke(points, {.width = 8.0F}, order);
            renderer.drawRect({0.0F, 0.0F, 4.0F, 4.0F}, math::Color::white(), order);
            renderer.drawText(*engine.getDefaultFont(), "Hi", {}, {}, order);
        }
    }));
    // clang-format on
    fixture.frames(2);

    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

TEST_F(MaterialTest, ReloadsItsShaderInPlace) {
    const std::string compiled(test::MaterialShaders::tint().begin(), test::MaterialShaders::tint().end());
    test::EngineFixture fixture({{"content/shaders/tint.shader", compiled}});
    assets::Manager& assets = fixture.engine().getAssets();
    const graphics::Shader shader = assets.shader("shaders/tint.shader");
    graphics2d::Material material(shader);
    material.set("strength", 0.25F);
    EXPECT_EQ(assets.shader("shaders/tint.shader"), shader);

    // A reload keeps every handle and the values that still fit, and the next draws create the programs again.
    fixture.package().setFile("content/shaders/tint.shader", test::TestFiles::bytes(compiled));
    EXPECT_EQ(assets.reload("shaders/tint.shader"), (assets::Manager::Reload{.inPlace = 1}));
    EXPECT_EQ(shader.getVersion(), 2U);
    EXPECT_EQ(material.get("strength"), std::vector<float>{0.25F});
    fixture.package().setFile("content/shaders/tint.shader", test::TestFiles::bytes("{}"));
    EXPECT_THROW((void)assets.reload("shaders/tint.shader"), std::invalid_argument);
    EXPECT_EQ(shader.getVersion(), 2U);
    EXPECT_THROW((void)assets.load("shader", "shaders/tint.shader", core::Json{{"filter", "linear"}}), std::invalid_argument);
}

} // namespace haylen::graphics2d
