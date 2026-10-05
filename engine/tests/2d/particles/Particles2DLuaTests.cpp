#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "support/EngineFixture.hpp"
#include "support/TestFiles.hpp"

namespace haylen::particles2d {

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
    EXPECT_NE(fixture.lua("particles2d.newEmitter({texture = graphics.whiteTexture(), bursts = {{time = 0, amount = 1}}})").find("Unknown option \"amount\""), std::string::npos);
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return graphics2d.stats().sprites"), "10");
    fixture.runLua("fire:configure({rate = 0}) fire.emitting = false fire:burst(2) fire:update(0)");
    EXPECT_EQ(fixture.lua("return fire.count .. ' ' .. tostring(fire.emitting)"), "12 false");

    // The seed only feeds the random numbers of a new emitter, so reconfiguring an emitter rejects it.
    EXPECT_NE(fixture.lua("fire:configure({rate = 5, seed = 3})").find("Unknown option \"seed\""), std::string::npos);
    EXPECT_EQ(fixture.lua("local c = fire.config return c.rate .. ' ' .. c.maxParticles .. ' ' .. c.lifetime[1] .. ',' .. c.lifetime[2] .. ' ' .. c.shape .. ' ' .. c.blend .. ' ' .. c.layer .. ' ' .. #c.colors .. ' ' .. #c.bursts .. ' ' .. tostring(c.texture == graphics.whiteTexture())"), "0.0 50 0.5,1.0 circle additive 3 2 0 true");
    EXPECT_EQ(fixture.lua("local copy = particles2d.newEmitter(fire.config) return copy.config.spread == fire.config.spread and copy.config.damping == fire.config.damping and copy.config.colors[2] == fire.config.colors[2]"), "true");

    // Positions are in world space unless the emitter keeps its particles local.
    EXPECT_EQ(fixture.lua("local positions = fire:positions() return #positions == fire.count and math.abs(positions[1].x - 10) < 40"), "true");
    fixture.runLua("fire:clear()");
    EXPECT_EQ(fixture.lua("return fire.count"), "0");
    EXPECT_EQ(fixture.lua("fire:burst(1e12) return fire.count"), "50");

    EXPECT_NE(fixture.lua("particles2d.newEmitter({texture = graphics.whiteTexture(), sped = 3})").find("Unknown option \"sped\""), std::string::npos);
    EXPECT_NE(fixture.lua("particles2d.newEmitter({texture = graphics.whiteTexture(), shape = 'fan'})").find("unknown value 'fan'"), std::string::npos);
    EXPECT_NE(fixture.lua("particles2d.newEmitter({})").find("needs a texture"), std::string::npos);
    EXPECT_NE(fixture.lua("fire:configure({lifetime = 0})").find("positive lifetime"), std::string::npos);
}

TEST(Particles2DLuaTest, ReadsAndReturnsEveryOption) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        particles2d = require('haylen.particles2d')
        graphics = require('haylen.graphics')
        spark = particles2d.newEmitter({texture = graphics.whiteTexture(), rate = 0, lifetime = 0.2})
        full = particles2d.newEmitter({
            texture = graphics.whiteTexture(), frameGrid = {columns = 1, rows = 1}, frameMode = 'loop', frameRate = 6,
            rate = 10, rateOverDistance = 0.2, bursts = {{time = 0, count = {2, 3}, cycles = 2, interval = 0.1, probability = 0.5}}, delay = 0.1, duration = 1,
            speedCurve = 'quadOut', directionMode = 'tangent', inheritVelocity = 0.4, turbulence = {strength = 20, frequency = 0.03, speed = 2},
            attractors = {{x = 1, y = 2, strength = 30, radius = 40, killRadius = 3, space = 'world'}},
            collision = {type = 'bounds', area = {-10, -10, 20, 20}, bounce = 0.8, friction = 0.1, result = 'bounce', lifeLoss = 0.2},
            bounds = {-100, -100, 200, 200}, boundsMode = 'wrap', endSizeScale = {1, 1.5}, sizeCurve = {curve = 'backOut', overshoot = 2},
            aspect = {1, 3}, stretch = 0.1, rotation = {0, 1}, rotationStep = 0.5, alignToVelocity = true, spinCurve = {steps = 3, position = 'start'},
            colors = {'#FFFF0000', '#FF00FF00'}, colorTimes = {0, 1}, colorBlend = 'steps', tints = {'#FFFFFFFF'}, tintMode = 'cycleTime',
            shape = 'polyline', shapePoints = {{0, 0}, {10, 0}}, shapeAngle = 0.2, shapeArc = {0, 2}, shapeThickness = 3, pixelSnap = 2,
            particleOrder = 'newestFirst', trail = {length = 5, lifetime = 0.3, widthStart = 6, widthEnd = 1, colors = {'#FFFFFFFF'}},
            light = {type = 'point', offset = {0, -5}, radius = 50, color = '#FFFFA040', intensity = 2, flicker = {speed = 3, amount = 0.4}, fade = 'count'},
            particleLights = {radius = 10, intensity = 0.3, max = 4}, sortOffset = 2, visibility = 4, emission = 0.5, unshaded = true, lightMask = 2,
            subEmitters = {{effect = spark.config, trigger = 'alive', count = {1, 2}, rate = 3, probability = 0.7, inheritVelocity = 0.1, inheritColor = true}},
        })
        config = full.config
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return config.frameGrid.columns .. config.frameMode .. config.frameRate .. ' ' .. config.bursts[1].count[2] .. ' ' .. config.bursts[1].cycles .. ' ' .. config.speedCurve .. ' ' .. config.directionMode"), "1loop6.0 3 2 quadOut tangent");
    EXPECT_EQ(fixture.lua("return config.turbulence.speed .. ' ' .. config.attractors[1].space .. ' ' .. config.collision.type .. ' ' .. config.collision.area.width .. ' ' .. config.bounds.width .. ' ' .. config.boundsMode"), "2.0 world bounds 20.0 200.0 wrap");
    EXPECT_EQ(fixture.lua("return config.endSizeScale[2] .. ' ' .. config.sizeCurve.curve .. ' ' .. config.sizeCurve.overshoot .. ' ' .. config.spinCurve.steps .. config.spinCurve.position .. ' ' .. config.aspect[2] .. ' ' .. tostring(config.alignToVelocity)"), "1.5 backOut 2.0 3start 3.0 true");
    EXPECT_EQ(fixture.lua("return config.colorBlend .. ' ' .. config.tintMode .. ' ' .. config.shape .. ' ' .. #config.shapePoints .. ' ' .. config.shapeThickness .. ' ' .. config.particleOrder .. ' ' .. config.trail.length"), "steps cycleTime polyline 2 3.0 newestFirst 5");
    EXPECT_EQ(fixture.lua("return string.format('%.1f', config.light.flicker.amount) .. ' ' .. config.light.fade .. ' ' .. config.particleLights.max .. ' ' .. config.visibility .. ' ' .. tostring(config.unshaded) .. ' ' .. config.lightMask"), "0.4 count 4 4 true 2");
    EXPECT_EQ(fixture.lua("local sub = config.subEmitters[1] return sub.trigger .. ' ' .. sub.count[2] .. ' ' .. tostring(sub.inheritColor) .. ' ' .. string.format('%.1f', sub.effect.lifetime[1])"), "alive 2 true 0.2");

    // A configuration goes back into a new emitter as it is, and `false` removes the optional parts.
    EXPECT_EQ(fixture.lua("local copy = particles2d.newEmitter(config) copy:update(0.5) return copy.config.light.radius .. ' ' .. #copy.config.subEmitters"), "50.0 1");
    EXPECT_EQ(fixture.lua("full:configure({light = false, particleLights = false, bounds = false, endSizeScale = false}) local c = full.config return tostring(c.light) .. ' ' .. tostring(c.particleLights) .. ' ' .. tostring(c.bounds) .. ' ' .. tostring(c.endSizeScale)"), "nil nil nil nil");

    EXPECT_NE(fixture.lua("full:configure({sizeCurve = function(t) return t end})").find("takes a curve name or a table, not a function"), std::string::npos);
    EXPECT_NE(fixture.lua("full:configure({turbulence = {strenght = 1}})").find("Unknown option \"strenght\""), std::string::npos);
    EXPECT_NE(fixture.lua("full:configure({collision = {type = 'sticky'}})").find("unknown value 'sticky'"), std::string::npos);
    EXPECT_NE(fixture.lua("full:configure({subEmitters = {{trigger = 'death'}}})").find("needs a \"ParticleEffect\" or an options table"), std::string::npos);
    EXPECT_NE(fixture.lua("full:configure({frameGrid = {columns = 2, rows = 1}, frames = {{0, 0, 1, 1}}})").find("either frames or a frame grid"), std::string::npos);
}

TEST(Particles2DLuaTest, ReadsParticlesAndSteersEmitters) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        particles2d = require('haylen.particles2d')
        graphics = require('haylen.graphics')
        physics2d = require('haylen.physics2d')
        coins = particles2d.newEmitter({
            texture = graphics.whiteTexture(), rate = 0, lifetime = 10, speed = 0, startSize = 6, endSize = 6, colors = {'#FFFFE040'},
            attractors = {{strength = 4000, radius = 1000, killRadius = 4, space = 'world'}},
        })
        coins.position = {0, 0}
        coins:setAttractor(1, 300, 0)
        coins:burst(1)
        coins:update(0.1)
        bolt = particles2d.newEmitter({texture = graphics.whiteTexture(), rate = 0, speed = 0, shape = 'polyline', shapePoints = {{0, 0}, {1, 0}}})
        bolt:setShapePoints({{0, 50}, {0, 100}})
        bolt:burst(5)
        world = physics2d.newWorld({gravity = {0, 0}})
        wall = world:createBody({type = 'static', x = 60})
        wall:addBox(10, 200)
        rain = particles2d.newEmitter({texture = graphics.whiteTexture(), rate = 0, lifetime = 5, speed = 300, direction = 0, spread = 0, collision = {type = 'world', result = 'die'}})
        rain:setCollisionWorld(world, {category = 1, mask = 1})
        rain:burst(3)
        for step = 1, 10 do rain:update(0.05) end
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("local p = coins:particle(1) return (p.x > 0) and p.vx > 0 and p.width == 6 and p.color:toHex() == '#FFFFE040' and p.lifetime == 10"), "true");
    EXPECT_EQ(fixture.lua("local p = bolt:particle(3) return math.abs(p.x) < 1e-4 and p.y >= 50 and p.y <= 100"), "true");
    EXPECT_EQ(fixture.lua("return rain.count"), "0");
    fixture.runLua("rain:setCollisionWorld(nil) rain:burst(3) for step = 1, 10 do rain:update(0.05) end");
    EXPECT_EQ(fixture.lua("return rain.count"), "3");

    fixture.runLua("coins.scale = 2 coins.rotation = 1.5 coins:burst(1)");
    EXPECT_EQ(fixture.lua("return coins.scale .. ' ' .. coins.rotation .. ' ' .. coins:particle(2).width"), "2.0 1.5 12.0");
    EXPECT_NE(fixture.lua("coins:particle(9)").find("no live particle at this index"), std::string::npos);
    EXPECT_NE(fixture.lua("coins:setAttractor(2, 0, 0)").find("no attractor at this index"), std::string::npos);
    EXPECT_NE(fixture.lua("bolt:setShapePoints({{0, 0}})").find("needs at least two points"), std::string::npos);
}

TEST(Particles2DLuaTest, CreatesSystemsTrailsAndImageShapes) {
    const std::vector<std::uint8_t> png = test::TestFiles::pngImage(4, 4, 0xFFFFFFFFU);
    const std::vector<std::uint8_t> faint = test::TestFiles::pngImage(2, 2, 0xFFFFFF20U);
    test::EngineFixture fixture({
        {"content/effects/spark.particles", R"({"texture": "../images/dot.png", "rate": 0, "bursts": [{"time": 0, "count": 4}], "duration": 0.1, "speed": 0})"},
        {"content/effects/blast.particles", R"({"emitters": [{"name": "core", "effect": "spark.particles"}, {"name": "ring", "effect": "spark.particles", "offset": [20, 0], "delay": 0.2}]})"},
        {"content/images/dot.png", std::string(png.begin(), png.end())},
        {"content/images/faint.png", std::string(faint.begin(), faint.end())},
    });
    // clang-format off
    fixture.runLua(R"(
        assets = require('haylen.assets')
        particles2d = require('haylen.particles2d')
        graphics = require('haylen.graphics')
        blast = assets.load('effects/blast.particles')
        system = particles2d.newSystem(blast, {seed = 3})
        system.position = {100, 0}
        system:update(0.05)
        trail = particles2d.newTrail({lifetime = 0.5, minDistance = 5, widthStart = 10, colors = {'#FFFFFFFF', '#00FFFFFF'}, layer = 2, blend = 'additive'})
        for step = 1, 5 do trail.x = step * 10 trail:update(0.05) end
        shape = assets.load('images/dot.png', 'imageShape', {source = {0, 0, 2, 4}})
        sparkle = particles2d.newEmitter({texture = graphics.whiteTexture(), rate = 0, speed = 0, shape = 'image', shapeImage = shape, shapeSize = {20, 40}})
        sparkle:burst(10)
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return tostring(blast.composite) .. ' ' .. tostring(blast.config) .. ' ' .. tostring(blast.texturePath) .. ' ' .. #blast.parts .. ' ' .. blast.parts[2].name .. ' ' .. blast.parts[2].offset.x .. ' ' .. blast.parts[2].texturePath"), "true nil nil 2 ring 20.0 images/dot.png");
    EXPECT_EQ(fixture.lua("return system.count .. ' ' .. tostring(system.alive) .. ' ' .. tostring(system.emitting) .. ' ' .. #system:emitters()"), "4 true true 2");
    EXPECT_EQ(fixture.lua("system:update(0.3) return system.count .. ' ' .. system:emitter('ring'):positions()[1].x .. ' ' .. tostring(system:emitter('smoke'))"), "8 120.0 nil");
    EXPECT_EQ(fixture.lua("system.emitting = false system:restart() return system.count .. ' ' .. tostring(system.emitting)"), "0 true");
    EXPECT_NE(fixture.lua("particles2d.newEmitter(blast)").find("create it with \"particles2d.newSystem\""), std::string::npos);
    EXPECT_EQ(fixture.lua("return particles2d.newSystem(assets.load('effects/spark.particles')).alive"), "true");

    EXPECT_EQ(fixture.lua("return trail.count .. ' ' .. trail.config.widthStart .. ' ' .. trail.config.blend .. ' ' .. trail.config.layer"), "5 10.0 additive 2");
    EXPECT_EQ(fixture.lua("trail:configure({widthStart = 4, maxPoints = 3}) return trail.count .. ' ' .. trail.config.widthStart .. ' ' .. trail.config.layer"), "3 4.0 2");
    EXPECT_EQ(fixture.lua("trail.emitting = false trail:update(1) return trail.count .. ' ' .. tostring(trail.alive)"), "0 false");
    EXPECT_NE(fixture.lua("particles2d.newTrail({lifetime = 0})").find("positive, finite lifetime"), std::string::npos);

    EXPECT_EQ(fixture.lua("return shape.count .. ' ' .. shape.width .. ' ' .. shape.height .. ' ' .. tostring(sparkle.config.shapeImage == shape)"), "8 2.0 4.0 true");
    EXPECT_EQ(fixture.lua("local x = sparkle:particle(1).x return x >= -10 and x <= 10"), "true");
    EXPECT_NE(fixture.lua("assets.load('images/faint.png', 'imageShape')").find("at least one pixel"), std::string::npos);
    EXPECT_NE(fixture.lua("assets.load('images/dot.png', 'imageShape', {edge = 1})").find("edge"), std::string::npos);
}

TEST(Particles2DLuaTest, LoadsEffectFilesForLua) {
    const std::vector<std::uint8_t> png = test::TestFiles::pngImage(8, 8, 0xFFFFFFFFU);
    test::EngineFixture fixture({
        {"content/effects/sparks.particles", R"({
            "texture": "../images/spark.png", "frames": [[0, 0, 4, 4], [4, 0, 4, 4]], "rate": 0, "bursts": [{"time": 0, "count": 12}],
            "duration": 0.5, "loop": true, "maxParticles": 64, "lifetime": [0.4, 0.6], "speed": 80, "gravity": [0, 200],
            "tangentialAcceleration": [0, 5], "endSize": 0, "colors": ["#FFFFE080", "#00FF4000"], "shape": "cone", "shapeSize": [6, 0],
            "localSpace": true, "layer": 4, "depth": 0.5, "blend": "additive"
        })"},
        {"content/images/spark.png", std::string(png.begin(), png.end())},
    });
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
    EXPECT_EQ(fixture.lua("return effect.texturePath .. ' ' .. tostring(effect.composite) .. ' ' .. tostring(config.texture == require('haylen.assets').texture('images/spark.png')) .. ' ' .. #config.frames .. ' ' .. config.frames[2].x"), "images/spark.png false true 2 4.0");
    EXPECT_EQ(fixture.lua("return tostring(effect == require('haylen.assets').load('effects/sparks.particles'))"), "true");
    EXPECT_EQ(fixture.lua("return config.bursts[1].time .. ':' .. config.bursts[1].count[1] .. ' ' .. tostring(config.loop) .. ' ' .. config.maxParticles .. ' ' .. config.speed[1] .. ',' .. config.speed[2] .. ' ' .. config.shape .. ' ' .. config.blend .. ' ' .. config.layer .. ' ' .. config.depth .. ' ' .. tostring(config.localSpace)"), "0.0:12 true 64 80.0,80.0 cone additive 4 0.5 true");
    EXPECT_EQ(fixture.lua("return config.gravity.y .. ' ' .. config.shapeSize.x .. ' ' .. config.colors[2]:toHex() .. ' ' .. config.tangentialAcceleration[2] .. ' ' .. config.endSize[1]"), "200.0 6.0 #00FF4000 5.0 0.0");
}

} // namespace haylen::particles2d
