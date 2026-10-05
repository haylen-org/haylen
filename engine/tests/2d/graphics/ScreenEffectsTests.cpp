#include <gtest/gtest.h>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/graphics/SpriteBatch.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "support/EngineFixture.hpp"
#include "support/FrameRenderer.hpp"

namespace haylen::graphics2d {

TEST(ScreenEffectsTest, CameraFlashFadesOverWorldCanvases) {
    Camera camera;
    EXPECT_EQ(camera.getFlash().a, 0.0F);
    camera.flash(math::Color{1.0F, 1.0F, 1.0F, 0.8F}, 0.5F);
    EXPECT_FLOAT_EQ(camera.getFlash().a, 0.8F);
    camera.update(0.25F);
    EXPECT_FLOAT_EQ(camera.getFlash().a, 0.2F);
    camera.update(0.5F);
    EXPECT_EQ(camera.getFlash(), math::Color::transparent());

    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(4, 4, math::Color::white()));
    camera.flash(math::Color::white(), 1.0F);
    // clang-format off
    const Renderer::Stats stats = test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) {
        Renderer& renderer = engine.getRenderer2D();
        renderer.beginWorld(camera);
        renderer.draw({.texture = texture});
        renderer.beginWorld(Camera{});
        renderer.draw({.texture = texture});
    });
    // clang-format on
    EXPECT_EQ(stats.sprites, 3U);
}

TEST(ScreenEffectsTest, DistortionDrawsGoIntoTheMapOfCompositedCanvases) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(4, 4, math::Color::white()));
    const DrawOrder bending{.distortion = 0.5F};

    // A plain canvas has no distortion map and skips the draws, and a composited one renders them in a pass of their own.
    // clang-format off
    const Renderer::Stats plain = test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) {
        Renderer& renderer = engine.getRenderer2D();
        renderer.beginWorld(Camera{});
        renderer.draw({.texture = texture, .order = bending});
        renderer.drawCircle({}, 20.0F, math::Color::white(), bending);
    });
    std::size_t visible = 0;
    const Renderer::Stats bent = test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) {
        Renderer& renderer = engine.getRenderer2D();
        renderer.beginWorld(Camera{}, {.postProcess = PostProcess{.distortion = 32.0F}});
        renderer.draw({.texture = texture});
        renderer.draw({.texture = texture, .order = bending});
        renderer.drawBatch(texture, std::vector<SpriteInstance>(3, SpriteInstance{.size = {8.0F, 8.0F}}), bending);
        renderer.drawCircle({}, 20.0F, math::Color::white(), bending);
        renderer.visitDrawn([&](const Renderer::Drawn&) { ++visible; });
    });
    const Renderer::Stats flat = test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) {
        Renderer& renderer = engine.getRenderer2D();
        renderer.beginWorld(Camera{}, {.postProcess = PostProcess{.distortion = 32.0F}});
        renderer.draw({.texture = texture});
    });
    // clang-format on
    EXPECT_EQ(plain.sprites, 0U);
    EXPECT_EQ(plain.vertices, 0U);
    EXPECT_EQ(bent.sprites, 5U);
    EXPECT_EQ(visible, 1U);
    EXPECT_EQ(bent.passes, flat.passes + 1U);

    Renderer& renderer = fixture.engine().getRenderer2D();
    renderer.beginWorld(Camera{}, {.ambientLight = math::Color::white()});
    const StaticSpriteBatch baked = renderer.createStaticBatch(texture, std::vector<SpriteInstance>(1, SpriteInstance{.size = {4.0F, 4.0F}}));
    EXPECT_THROW(renderer.drawStatic(baked, bending), std::invalid_argument);
    EXPECT_THROW(renderer.drawText(*fixture.engine().getDefaultFont(), "Bent", {}, {}, bending), std::invalid_argument);
    fixture.frames(1);
}

TEST(ScreenEffectsTest, BloomAndBlurAddPassesAtHalfTheSize) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(4, 4, math::Color::white()));
    // clang-format off
    const auto render = [&](const PostProcess& post) {
        return test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) {
            Renderer& renderer = engine.getRenderer2D();
            renderer.beginWorld(Camera{}, {.postProcess = post});
            renderer.draw({.texture = texture});
        });
    };
    // clang-format on
    const Renderer::Stats plain = render({});
    const Renderer::Stats lens = render({.chromaticAberration = 4.0F, .pixelate = 6.0F});
    const Renderer::Stats bloom = render({.bloomStrength = 0.8F});
    const Renderer::Stats both = render({.blur = 8.0F, .bloomStrength = 0.8F});

    // The lens effects live in the composite, and bloom and blur each shrink and blur twice after the first stage.
    EXPECT_EQ(lens.passes, plain.passes);
    EXPECT_EQ(bloom.passes, plain.passes + 4U);
    EXPECT_EQ(both.passes, plain.passes + 7U);
    EXPECT_EQ(both.drawCalls, plain.drawCalls + 7U);

    // A lookup texture is as wide as its height squared.
    const graphics::Texture lut = fixture.engine().getGraphics().createTexture(graphics::Image(16, 4, math::Color::white()));
    const graphics::Texture wrong = fixture.engine().getGraphics().createTexture(graphics::Image(16, 16, math::Color::white()));
    EXPECT_EQ(render({.colorLut = lut, .colorLutStrength = 0.5F}).passes, plain.passes);
    Renderer& renderer = fixture.engine().getRenderer2D();
    EXPECT_THROW(renderer.beginWorld(Camera{}, {.postProcess = PostProcess{.colorLut = wrong}}), std::invalid_argument);
    EXPECT_THROW(renderer.beginWorld(Camera{}, {.postProcess = PostProcess{.colorLut = lut, .colorLutStrength = 2.0F}}), std::invalid_argument);
    EXPECT_THROW(renderer.beginWorld(Camera{}, {.postProcess = PostProcess{.bloomRadius = 0.0F}}), std::invalid_argument);
    EXPECT_THROW(renderer.beginWorld(Camera{}, {.postProcess = PostProcess{.blur = -1.0F}}), std::invalid_argument);
}

TEST(ScreenEffectsTest, SpriteEffectsDrawInOneCallPerTexture) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(16, 16, math::Color::white()));
    // clang-format off
    const Renderer::Stats stats = test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) {
        Renderer& renderer = engine.getRenderer2D();
        renderer.beginWorld(Camera{});
        for (int index = 0; index < 4; ++index) {
            renderer.draw({.texture = texture, .position = {static_cast<float>(index) * 20.0F, 0.0F}, .effect = {.dissolve = 0.25F * static_cast<float>(index), .outlineWidth = 2.0F, .glowSize = static_cast<float>(index) * 3.0F}});
        }
        renderer.draw({.texture = texture});
        renderer.beginWorld(Camera{}, {.ambientLight = math::Color::black()});
        renderer.draw({.texture = texture, .effect = {.glowSize = 8.0F, .glowColor = math::Color{1.0F, 0.8F, 0.2F, 1.0F}}});
    });
    // clang-format on
    EXPECT_EQ(stats.sprites, 6U);
    EXPECT_EQ(stats.instances, 11U);

    Renderer& renderer = fixture.engine().getRenderer2D();
    renderer.beginScreen();
    EXPECT_THROW(renderer.draw({.texture = texture, .effect = {.dissolve = 2.0F}}), std::invalid_argument);
    EXPECT_THROW(renderer.draw({.texture = texture, .effect = {.outlineWidth = 65.0F}}), std::invalid_argument);
    EXPECT_THROW(renderer.draw({.texture = texture, .effect = {.dissolve = 0.5F}, .order = {.partMask = texture}}), std::invalid_argument);
    fixture.frames(1);
}

TEST(ScreenEffectsTest, LuaReachesFlashDistortionPostProcessingAndSpriteEffects) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        graphics = require('haylen.graphics')
        graphics2d = require('haylen.graphics2d')
        camera = graphics2d.newCamera()
        camera:flash('#C0FFFFFF', 0.5)
        hero = graphics2d.newSprite(graphics.whiteTexture(), {x = 10, width = 32, height = 32, effect = {dissolve = 0.5, dissolveColor = '#FFFF8000', outlineWidth = 2}})
        lut = graphics.newTexture(16, 4)
        require('haylen.scene').push({render = function()
            graphics2d.beginWorld(camera, {postProcess = {distortion = 20, chromaticAberration = 3, pixelate = 4, blur = 6, bloomStrength = 0.6, bloomThreshold = 0.7, bloomRadius = 10, colorLut = lut, colorLutStrength = 0.5}})
            lit = graphics2d.canvasLit()
            hero:draw()
            graphics2d.draw(graphics.whiteTexture(), 0, 0, {width = 40, height = 40, distortion = 0.8})
            graphics2d.draw(graphics.whiteTexture(), 0, 0, {effect = {glowSize = 6, glowColor = '#FFFFD040'}})
        end})
    )");
    // clang-format on
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return tostring(lit) .. ' ' .. graphics2d.stats().sprites"), "false 4");
    EXPECT_EQ(fixture.lua("return camera:flashColor().a > 0 and camera:flashColor().a < 0.76"), "true");
    EXPECT_EQ(fixture.lua("local e = hero.effect return e.dissolve .. ' ' .. e.dissolveColor:toHex() .. ' ' .. e.outlineWidth .. ' ' .. e.glowSize"), "0.5 #FFFF8000 2.0 0.0");
    EXPECT_EQ(fixture.lua("hero.effect = {glowSize = 4} hero.distortion = 0.5 return hero.effect.glowSize .. ' ' .. hero.distortion"), "4.0 0.5");
    EXPECT_NE(fixture.lua("hero.effect = {dissolve = 3}").find("dissolve and a dissolve edge from 0 to 1"), std::string::npos);
    EXPECT_NE(fixture.lua("hero.effect = {melt = 1}").find("Unknown option \"melt\""), std::string::npos);
}

} // namespace haylen::graphics2d
