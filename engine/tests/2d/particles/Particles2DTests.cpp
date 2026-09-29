#include <gtest/gtest.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/particles/Effect.hpp"
#include "haylen/2d/particles/Emitter.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::particles2d {

TEST(EmitterTest, EmitsMovesAndExpiresParticles) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(8, 8, math::Color::white()));

    particles2d::Emitter emitter({.texture = texture, .rate = 10.0F, .maxParticles = 5, .lifetime = {1.0F, 1.0F}, .speed = {10.0F, 10.0F}, .direction = 0.0F, .spread = 0.0F, .gravity = {0.0F, 20.0F}}, 7);
    emitter.position = {100.0F, 100.0F};
    emitter.update(0.25F);
    EXPECT_EQ(emitter.getCount(), 2U);
    emitter.update(0.25F);
    EXPECT_EQ(emitter.getCount(), 5U);
    emitter.update(1.0F);
    EXPECT_EQ(emitter.getCount(), 5U);
    EXPECT_TRUE(emitter.isAlive());

    emitter.emitting = false;
    emitter.update(1.5F);
    EXPECT_EQ(emitter.getCount(), 0U);
    EXPECT_FALSE(emitter.isAlive());

    emitter.burst(3);
    EXPECT_EQ(emitter.getCount(), 3U);
    emitter.clear();
    EXPECT_EQ(emitter.getCount(), 0U);
    EXPECT_THROW(emitter.setConfig({}), std::invalid_argument);
}

TEST(EmitterTest, DrawsEveryShapeAndColorRamp) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(16, 8, math::Color::white()));

    std::vector<particles2d::Emitter> emitters;
    for (const particles2d::EmitterConfig::Shape shape : {particles2d::EmitterConfig::Shape::Point, particles2d::EmitterConfig::Shape::Circle, particles2d::EmitterConfig::Shape::Ring, particles2d::EmitterConfig::Shape::Rectangle}) {
        particles2d::EmitterConfig config{.texture = texture, .frames = {{0.0F, 0.0F, 8.0F, 8.0F}, {8.0F, 0.0F, 8.0F, 8.0F}}, .rate = 0.0F, .lifetime = {0.5F, 1.0F}, .startSize = {4.0F, 8.0F}, .endSize = {0.0F, 0.0F}, .spin = {-1.0F, 1.0F}, .colors = {math::Color::white(), math::Color{1.0F, 0.5F, 0.0F, 1.0F}, math::Color::transparent()}, .shape = shape, .shapeSize = {20.0F, 10.0F}, .localSpace = shape == particles2d::EmitterConfig::Shape::Ring, .order = {.layer = 2, .blend = graphics::BlendMode::Type::Additive}};
        emitters.emplace_back(config, 3);
        emitters.back().burst(10);
        emitters.back().update(0.3F);
    }

    // clang-format off
    fixture.engine().getScenes().push(std::make_shared<test::DrawingScene>([&](core::Engine& engine) {
        engine.getRenderer2D().beginWorld(graphics2d::Camera{});
        for (const particles2d::Emitter& emitter : emitters) {
            emitter.draw(engine.getRenderer2D());
        }
        particles2d::Emitter({.texture = texture}).draw(engine.getRenderer2D());
    }));
    // clang-format on
    fixture.frames(1);
    EXPECT_EQ(fixture.engine().getRenderer2D().getStats().sprites, 40U);

    EXPECT_EQ(particles2d::EmitterConfig::shapeFromName("ring"), particles2d::EmitterConfig::Shape::Ring);
    EXPECT_EQ(particles2d::EmitterConfig::shapeFromName("cone"), particles2d::EmitterConfig::Shape::Cone);
    EXPECT_FALSE(particles2d::EmitterConfig::shapeFromName("fan").has_value());
    EXPECT_EQ(particles2d::EmitterConfig::shapeName(particles2d::EmitterConfig::Shape::Rectangle), "rectangle");
    EXPECT_THROW(particles2d::Emitter({.texture = texture, .lifetime = {0.0F, 1.0F}}), std::invalid_argument);
    EXPECT_THROW(particles2d::Emitter({.texture = texture, .colors = {}}), std::invalid_argument);
}

TEST(EmitterTest, RunsEmissionCyclesWithBurstsAndLoops) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(4, 4, math::Color::white()));

    particles2d::Emitter once({.texture = texture, .rate = 0.0F, .bursts = {{.time = 0.5F, .count = 3}, {.time = 0.0F, .count = 5}}, .duration = 1.0F, .lifetime = {10.0F, 10.0F}});
    once.update(0.1F);
    EXPECT_EQ(once.getCount(), 5U);
    once.update(0.5F);
    EXPECT_EQ(once.getCount(), 8U);
    EXPECT_TRUE(once.emitting);
    once.update(0.5F);
    EXPECT_FALSE(once.emitting);
    EXPECT_EQ(once.getCycleTime(), 1.0F);
    EXPECT_TRUE(once.isAlive());
    once.restart();
    EXPECT_EQ(once.getCount(), 0U);
    EXPECT_TRUE(once.emitting);
    EXPECT_EQ(once.getCycleTime(), 0.0F);

    // One long frame runs every cycle it spans, firing the bursts of each.
    particles2d::Emitter looping({.texture = texture, .rate = 0.0F, .bursts = {{.time = 0.0F, .count = 2}}, .duration = 1.0F, .loop = true, .lifetime = {10.0F, 10.0F}});
    looping.update(2.5F);
    EXPECT_EQ(looping.getCount(), 6U);
    EXPECT_FLOAT_EQ(looping.getCycleTime(), 0.5F);
    EXPECT_TRUE(looping.emitting);

    particles2d::Emitter timed({.texture = texture, .rate = 10.0F, .duration = 0.5F, .lifetime = {10.0F, 10.0F}});
    timed.update(1.0F);
    EXPECT_EQ(timed.getCount(), 5U);
    EXPECT_FALSE(timed.emitting);

    EXPECT_THROW(particles2d::Emitter({.texture = texture, .loop = true}), std::invalid_argument);
    EXPECT_THROW(particles2d::Emitter({.texture = texture, .bursts = {{.time = 2.0F, .count = 1}}, .duration = 1.0F}), std::invalid_argument);
    EXPECT_THROW(particles2d::Emitter({.texture = texture, .prewarm = -1.0F}), std::invalid_argument);
}

TEST(EmitterTest, PrewarmsConesAndAcceleratesAroundTheEmitter) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(4, 4, math::Color::white()));

    // Prewarming waits for the first update, so it starts where the app placed the emitter.
    particles2d::Emitter warm({.texture = texture, .rate = 10.0F, .prewarm = 1.0F, .lifetime = {5.0F, 5.0F}, .speed = {0.0F, 0.0F}}, 1);
    EXPECT_EQ(warm.getCount(), 0U);
    warm.position = {300.0F, 200.0F};
    warm.update(0.0F);
    EXPECT_NEAR(static_cast<double>(warm.getCount()), 10.0, 1.0);
    EXPECT_EQ(warm.getPositions().front(), math::Vec2(300.0F, 200.0F));

    particles2d::Emitter cone({.texture = texture, .rate = 0.0F, .lifetime = {5.0F, 5.0F}, .speed = {10.0F, 10.0F}, .direction = 0.0F, .spread = 0.0F, .shape = particles2d::EmitterConfig::Shape::Cone, .shapeSize = {8.0F, 0.0F}}, 2);
    cone.burst(20);
    for (const math::Vec2 point : cone.getPositions()) {
        EXPECT_GE(point.x, 0.0F);
        EXPECT_LE(point.x, 8.0F);
        EXPECT_NEAR(point.y, 0.0F, 1e-5F);
    }

    particles2d::Emitter radial({.texture = texture, .rate = 0.0F, .lifetime = {5.0F, 5.0F}, .speed = {0.0F, 0.0F}, .radialAcceleration = {100.0F, 100.0F}, .shape = particles2d::EmitterConfig::Shape::Ring, .shapeSize = {10.0F, 0.0F}}, 3);
    radial.position = {50.0F, 50.0F};
    radial.burst(8);
    radial.update(0.1F);
    for (const math::Vec2 point : radial.getPositions()) {
        EXPECT_NEAR((point - radial.position).getLength(), 11.0F, 1e-3F);
    }

    particles2d::Emitter swirl({.texture = texture, .rate = 0.0F, .lifetime = {5.0F, 5.0F}, .speed = {0.0F, 0.0F}, .tangentialAcceleration = {100.0F, 100.0F}, .shape = particles2d::EmitterConfig::Shape::Ring, .shapeSize = {10.0F, 0.0F}, .localSpace = true}, 4);
    swirl.burst(1);
    const math::Vec2 before = swirl.getPositions().front();
    swirl.update(0.1F);
    const math::Vec2 after = swirl.getPositions().front();
    EXPECT_GT(before.x * after.y - before.y * after.x, 0.0F);
}

TEST(EmitterTest, UpdatesLargeEmittersInParallel) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(4, 4, math::Color::white()));
    const particles2d::EmitterConfig config{.texture = texture, .rate = 0.0F, .maxParticles = 20000, .lifetime = {0.5F, 2.0F}, .gravity = {0.0F, 30.0F}, .radialAcceleration = {-20.0F, 20.0F}, .tangentialAcceleration = {5.0F, 15.0F}, .damping = 0.3F, .shape = particles2d::EmitterConfig::Shape::Circle, .shapeSize = {40.0F, 0.0F}};

    particles2d::Emitter serial(config, 9);
    particles2d::Emitter parallel(config, 9);
    serial.burst(20000);
    parallel.burst(20000);
    for (int frame = 0; frame < 30; ++frame) {
        serial.update(1.0F / 30.0F);
        parallel.update(1.0F / 30.0F, fixture.engine().getJobs());
    }
    ASSERT_EQ(serial.getCount(), parallel.getCount());
    EXPECT_GT(serial.getCount(), 0U);
    EXPECT_LT(serial.getCount(), 20000U);
    EXPECT_TRUE(std::equal(serial.getPositions().begin(), serial.getPositions().end(), parallel.getPositions().begin()));
}

TEST(EffectTest, LoadsEffectFilesAsAssets) {
    const std::vector<std::uint8_t> png = test::pngImage(8, 8, 0xFFFFFFFFU);
    test::EngineFixture fixture({
        {"content/effects/sparks.particles", R"({
            "texture": "../images/spark.png", "frames": [[0, 0, 4, 4], [4, 0, 4, 4]], "rate": 0, "bursts": [{"time": 0, "count": 12}],
            "duration": 0.5, "loop": true, "prewarm": 0, "maxParticles": 64, "lifetime": [0.4, 0.6], "speed": 80, "direction": -1.57, "spread": 1,
            "gravity": [0, 200], "radialAcceleration": 10, "tangentialAcceleration": [0, 5], "damping": 0.1, "startSize": [4, 6], "endSize": 0,
            "spin": [-2, 2], "colors": ["#FFFFE080", "#00FF4000"], "shape": "cone", "shapeSize": [6, 0], "localSpace": true, "layer": 4, "depth": 0.5, "blend": "additive"
        })"},
        {"content/effects/broken.particles", R"({"texture": "../images/spark.png", "sped": 3})"},
        {"content/images/spark.png", std::string(png.begin(), png.end())},
    });

    const auto effect = std::static_pointer_cast<particles2d::Effect>(fixture.engine().getAssets().load("particles", "effects/sparks.particles"));
    EXPECT_EQ(effect->texturePath, "images/spark.png");
    EXPECT_EQ(effect->config.texture, fixture.engine().getAssets().texture("images/spark.png"));
    EXPECT_EQ(effect->config.bursts.front().count, 12U);
    EXPECT_TRUE(effect->config.loop);
    EXPECT_EQ(effect->config.shape, particles2d::EmitterConfig::Shape::Cone);
    EXPECT_EQ(effect->config.tangentialAcceleration.max, 5.0F);
    EXPECT_EQ(effect->config.colors.back(), math::Color::parse("#00FF4000"));
    EXPECT_EQ(effect->config.order.blend, graphics::BlendMode::Type::Additive);
    EXPECT_EQ(effect->config.order.layer, 4);
    EXPECT_EQ(fixture.engine().getAssets().getTypeForPath("effects/sparks.particles"), "particles");
    EXPECT_THROW((void)fixture.engine().getAssets().load("particles", "effects/broken.particles"), std::invalid_argument);

    // clang-format off
    fixture.runLua(R"(
        particles2d = require('haylen.particles2d')
        sparks = particles2d.newEmitter(require('haylen.assets').load('effects/sparks.particles'), {seed = 5, rate = 2})
        sparks:update(0.1)
        plain = particles2d.newEmitter(require('haylen.assets').load('effects/sparks.particles'))
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return sparks.count .. ' ' .. tostring(sparks.emitting) .. ' ' .. tostring(math.abs(sparks.cycleTime - 0.1) < 1e-6)"), "12 true true");
    EXPECT_EQ(fixture.lua("sparks:restart() return sparks.count .. ' ' .. sparks.cycleTime"), "0 0.0");
    EXPECT_EQ(fixture.lua("plain:update(0.2) return plain.count"), "12");

    // Overrides change only the keys they name, so the draw order of the effect survives them.
    EXPECT_EQ(fixture.lua("return sparks.config.blend .. ' ' .. sparks.config.layer .. ' ' .. sparks.config.depth .. ' ' .. sparks.config.rate"), "additive 4 0.5 2.0");

    fixture.runLua("effect = require('haylen.assets').load('effects/sparks.particles') config = effect.config");
    EXPECT_EQ(fixture.lua("return effect.texturePath .. ' ' .. tostring(config.texture == require('haylen.assets').texture('images/spark.png')) .. ' ' .. #config.frames .. ' ' .. config.frames[2].x"), "images/spark.png true 2 4.0");
    EXPECT_EQ(fixture.lua("return tostring(effect == require('haylen.assets').load('effects/sparks.particles'))"), "true");
    EXPECT_EQ(fixture.lua("return config.bursts[1].time .. ':' .. config.bursts[1].count .. ' ' .. tostring(config.loop) .. ' ' .. config.maxParticles .. ' ' .. config.speed[1] .. ',' .. config.speed[2] .. ' ' .. config.shape .. ' ' .. config.blend .. ' ' .. config.layer .. ' ' .. config.depth .. ' ' .. tostring(config.localSpace)"), "0.0:12 true 64 80.0,80.0 cone additive 4 0.5 true");
    EXPECT_EQ(fixture.lua("return config.gravity.y .. ' ' .. config.shapeSize.x .. ' ' .. config.colors[2]:toHex() .. ' ' .. config.tangentialAcceleration[2] .. ' ' .. config.endSize[1]"), "200.0 6.0 #00FF4000 5.0 0.0");
}

TEST(Particles2DLuaTest, CreatesAndConfiguresEmittersFromLua) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        particles2d = require('haylen.particles2d')
        graphics = require('haylen.graphics')
        graphics2d = require('haylen.graphics2d')
        fire = particles2d.newEmitter({
            texture = graphics.whiteTexture(), frames = {{0, 0, 1, 1}}, rate = 20, maxParticles = 50,
            lifetime = {0.5, 1}, speed = 40, direction = -1.57, spread = 0.4, gravity = {0, -10}, damping = 0.5,
            startSize = {8, 12}, endSize = 0, spin = {-1, 1}, colors = {'#FFFFAA00', '#00FF0000'},
            shape = 'circle', shapeSize = {6, 6}, localSpace = false, layer = 3, blend = 'additive', seed = 11,
        })
        fire.x = 10 fire.y = 20
        fire:update(0.5)
        blast = particles2d.newEmitter({
            texture = graphics.whiteTexture(), rate = 0, bursts = {{time = 0, count = 4}, {time = 0.2, count = 2}}, duration = 0.3,
            prewarm = 0, radialAcceleration = {5, 10}, tangentialAcceleration = 3, shape = 'cone', shapeSize = {4, 0},
        })
        blast:update(0.4)
        require('haylen.scene').push({render = function() graphics2d.beginWorld(graphics2d.newCamera()) fire:draw() end})
    )");
    // clang-format on

    EXPECT_EQ(fixture.lua("return fire.count .. ' ' .. tostring(fire.alive) .. ' ' .. fire.position.x"), "10 true 10.0");
    EXPECT_EQ(fixture.lua("return blast.count .. ' ' .. tostring(blast.emitting)"), "6 false");
    EXPECT_NE(fixture.lua("particles2d.newEmitter({texture = graphics.whiteTexture(), bursts = {{time = 0, amount = 1}}})").find("Unknown option 'amount'"), std::string::npos);
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return graphics2d.stats().sprites"), "10");
    fixture.runLua("fire:configure({rate = 0}) fire.emitting = false fire:burst(2) fire:update(0)");
    EXPECT_EQ(fixture.lua("return fire.count .. ' ' .. tostring(fire.emitting)"), "12 false");

    // The seed only feeds the random numbers of a new emitter, so reconfiguring an emitter rejects it.
    EXPECT_NE(fixture.lua("fire:configure({rate = 5, seed = 3})").find("Unknown option 'seed'"), std::string::npos);
    EXPECT_EQ(fixture.lua("local c = fire.config return c.rate .. ' ' .. c.maxParticles .. ' ' .. c.lifetime[1] .. ',' .. c.lifetime[2] .. ' ' .. c.shape .. ' ' .. c.blend .. ' ' .. c.layer .. ' ' .. #c.colors .. ' ' .. #c.bursts .. ' ' .. tostring(c.texture == graphics.whiteTexture())"), "0.0 50 0.5,1.0 circle additive 3 2 0 true");
    EXPECT_EQ(fixture.lua("local copy = particles2d.newEmitter(fire.config) return copy.config.spread == fire.config.spread and copy.config.damping == fire.config.damping and copy.config.colors[2] == fire.config.colors[2]"), "true");

    // Positions are in world space unless the emitter keeps its particles local.
    EXPECT_EQ(fixture.lua("local positions = fire:positions() return #positions == fire.count and math.abs(positions[1].x - 10) < 40"), "true");
    fixture.runLua("fire:clear()");
    EXPECT_EQ(fixture.lua("return fire.count"), "0");

    EXPECT_NE(fixture.lua("particles2d.newEmitter({texture = graphics.whiteTexture(), sped = 3})").find("Unknown option 'sped'"), std::string::npos);
    EXPECT_NE(fixture.lua("particles2d.newEmitter({texture = graphics.whiteTexture(), shape = 'fan'})").find("unknown value 'fan'"), std::string::npos);
    EXPECT_NE(fixture.lua("particles2d.newEmitter({})").find("needs a texture"), std::string::npos);
    EXPECT_NE(fixture.lua("fire:configure({lifetime = 0})").find("positive lifetime"), std::string::npos);
}

} // namespace haylen::particles2d
