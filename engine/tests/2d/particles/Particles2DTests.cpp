#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/particles/Effect.hpp"
#include "haylen/2d/particles/Emitter.hpp"
#include "haylen/2d/particles/ImageShape.hpp"
#include "haylen/2d/particles/Ribbon.hpp"
#include "haylen/2d/particles/System.hpp"
#include "haylen/2d/particles/Trail.hpp"
#include "haylen/2d/physics/World.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "support/DrawingScene.hpp"
#include "support/EngineFixture.hpp"
#include "support/TestFiles.hpp"

namespace haylen::particles2d {

class ParticlesTest : public ::testing::Test {
  protected:
    [[nodiscard]] graphics::Texture texture(int width = 4, int height = 4) {
        return fixture.engine().getGraphics().createTexture(graphics::Image(width, height, math::Color::white()));
    }

    // Draws the emitters in a world canvas, lit when an ambient light is given, and returns the statistics of the frame.
    graphics2d::Renderer::Stats drawFrame(const std::vector<const Emitter*>& emitters, std::optional<math::Color> ambient = std::nullopt) {
        // clang-format off
        fixture.engine().getScenes().push(std::make_shared<test::DrawingScene>([&emitters, ambient](core::Engine& engine) {
            engine.getRenderer2D().beginWorld(graphics2d::Camera{}, {.ambientLight = ambient});
            for (const Emitter* emitter : emitters) {
                emitter->draw(engine.getRenderer2D());
            }
        }));
        // clang-format on
        fixture.frames(1);
        const graphics2d::Renderer::Stats stats = fixture.engine().getRenderer2D().getStats();
        fixture.engine().getScenes().pop();
        fixture.frames(1);
        return stats;
    }

    test::EngineFixture fixture;
};

TEST_F(ParticlesTest, EmitsMovesAndExpiresParticles) {
    Emitter emitter({.texture = texture(), .rate = 10.0F, .maxParticles = 5, .lifetime = {1.0F, 1.0F}, .speed = {10.0F, 10.0F}, .direction = 0.0F, .spread = 0.0F, .gravity = {0.0F, 20.0F}}, 7);
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
    EXPECT_EQ(emitter.getParticle(0).position, emitter.position);
    EXPECT_THROW((void)emitter.getParticle(3), std::out_of_range);
    emitter.clear();
    EXPECT_EQ(emitter.getCount(), 0U);
    EXPECT_THROW(emitter.setConfig({}), std::invalid_argument);
}

TEST_F(ParticlesTest, DrawsEveryShapeAndColorRamp) {
    const graphics::Texture sheet = texture(16, 8);
    std::vector<std::unique_ptr<Emitter>> emitters;
    for (const EmitterConfig::Shape shape : {EmitterConfig::Shape::Point, EmitterConfig::Shape::Circle, EmitterConfig::Shape::Ring, EmitterConfig::Shape::Rectangle}) {
        const EmitterConfig config{.texture = sheet, .frames = {{0.0F, 0.0F, 8.0F, 8.0F}, {8.0F, 0.0F, 8.0F, 8.0F}}, .rate = 0.0F, .lifetime = {0.5F, 1.0F}, .startSize = {4.0F, 8.0F}, .endSize = {0.0F, 0.0F}, .spin = {-1.0F, 1.0F}, .colors = {math::Color::white(), math::Color{1.0F, 0.5F, 0.0F, 1.0F}, math::Color::transparent()}, .shape = shape, .shapeSize = {20.0F, 10.0F}, .localSpace = shape == EmitterConfig::Shape::Ring, .order = {.layer = 2, .blend = graphics::BlendMode::Type::Additive}};
        emitters.push_back(std::make_unique<Emitter>(config, 3));
        emitters.back()->burst(10);
        emitters.back()->update(0.3F);
    }
    const Emitter empty({.texture = sheet});
    std::vector<const Emitter*> drawn{&empty};
    for (const std::unique_ptr<Emitter>& emitter : emitters) {
        drawn.push_back(emitter.get());
    }
    EXPECT_EQ(drawFrame(drawn).sprites, 40U);

    EXPECT_EQ(EmitterConfig::fromName(EmitterConfig::kShapeNames, "ring"), EmitterConfig::Shape::Ring);
    EXPECT_EQ(EmitterConfig::fromName(EmitterConfig::kShapeNames, "rectangleEdge"), EmitterConfig::Shape::RectangleEdge);
    EXPECT_FALSE(EmitterConfig::fromName(EmitterConfig::kShapeNames, "fan").has_value());
    EXPECT_EQ(EmitterConfig::nameOf(EmitterConfig::kShapeNames, EmitterConfig::Shape::Rectangle), "rectangle");
    EXPECT_EQ(EmitterConfig::nameOf(EmitterConfig::kFrameModeNames, EmitterConfig::FrameMode::LoopRandomStart), "loopRandomStart");
    EXPECT_THROW(Emitter({.texture = sheet, .lifetime = {0.0F, 1.0F}}), std::invalid_argument);
    EXPECT_THROW(Emitter({.texture = sheet, .colors = {}}), std::invalid_argument);
}

TEST_F(ParticlesTest, RunsEmissionCyclesWithBurstsAndLoops) {
    Emitter once({.texture = texture(), .rate = 0.0F, .bursts = {{.time = 0.5F, .count = {3, 3}}, {.time = 0.0F, .count = {5, 5}}}, .duration = 1.0F, .lifetime = {10.0F, 10.0F}});
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
    Emitter looping({.texture = texture(), .rate = 0.0F, .bursts = {{.time = 0.0F, .count = {2, 2}}}, .duration = 1.0F, .loop = true, .lifetime = {10.0F, 10.0F}});
    looping.update(2.5F);
    EXPECT_EQ(looping.getCount(), 6U);
    EXPECT_FLOAT_EQ(looping.getCycleTime(), 0.5F);
    EXPECT_TRUE(looping.emitting);

    Emitter timed({.texture = texture(), .rate = 10.0F, .duration = 0.5F, .lifetime = {10.0F, 10.0F}});
    timed.update(1.0F);
    EXPECT_EQ(timed.getCount(), 5U);
    EXPECT_FALSE(timed.emitting);

    EXPECT_THROW(Emitter({.texture = texture(), .loop = true}), std::invalid_argument);
    EXPECT_THROW(Emitter({.texture = texture(), .bursts = {{.time = 2.0F, .count = {1, 1}}}, .duration = 1.0F}), std::invalid_argument);
    EXPECT_THROW(Emitter({.texture = texture(), .prewarm = -1.0F}), std::invalid_argument);
}

TEST_F(ParticlesTest, KeepsHugeRatesBurstsAndFramesWithinItsRoom) {
    // Spawning stops at `maxParticles` at once, however many particles a burst or a rate asks for.
    Emitter flood({.texture = texture(), .rate = 1.0e30F, .maxParticles = 5, .lifetime = {10.0F, 10.0F}});
    flood.update(0.1F);
    EXPECT_EQ(flood.getCount(), 5U);
    flood.clear();
    flood.burst(std::numeric_limits<std::size_t>::max());
    EXPECT_EQ(flood.getCount(), 5U);

    // A frame far longer than the loop counts its whole loops at once, bursts included.
    Emitter looping({.texture = texture(), .rate = 0.0F, .bursts = {{.time = 0.0F, .count = {1, 1}}}, .duration = 0.01F, .loop = true, .maxParticles = 1000, .lifetime = {1.0e7F, 1.0e7F}});
    looping.update(1.0e6F);
    EXPECT_EQ(looping.getCount(), 1000U);
    EXPECT_GE(looping.getCycleTime(), 0.0F);
    EXPECT_LE(looping.getCycleTime(), 0.01F);

    // Values that cannot end in a frame are rejected: infinite rates and durations, loops shorter than a millisecond, prewarm times beyond a minute and more than a million particles.
    const float infinity = std::numeric_limits<float>::infinity();
    EXPECT_THROW(Emitter({.texture = texture(), .rate = infinity}), std::invalid_argument);
    EXPECT_THROW(Emitter({.texture = texture(), .duration = infinity}), std::invalid_argument);
    EXPECT_THROW(Emitter({.texture = texture(), .duration = 1.0e-10F, .loop = true}), std::invalid_argument);
    EXPECT_THROW(Emitter({.texture = texture(), .prewarm = 61.0F}), std::invalid_argument);
    EXPECT_THROW(Emitter({.texture = texture(), .maxParticles = 1000001}), std::invalid_argument);
    EXPECT_NO_THROW(Emitter({.texture = texture(), .duration = 0.001F, .loop = true, .prewarm = 60.0F}));
}

TEST_F(ParticlesTest, PrewarmsConesAndAcceleratesAroundTheEmitter) {
    // Prewarming waits for the first update, so it starts where the app placed the emitter.
    Emitter warm({.texture = texture(), .rate = 10.0F, .prewarm = 1.0F, .lifetime = {5.0F, 5.0F}, .speed = {0.0F, 0.0F}}, 1);
    EXPECT_EQ(warm.getCount(), 0U);
    warm.position = {300.0F, 200.0F};
    warm.update(0.0F);
    EXPECT_NEAR(static_cast<double>(warm.getCount()), 10.0, 1.0);
    EXPECT_EQ(warm.getPositions().front(), math::Vec2(300.0F, 200.0F));

    Emitter cone({.texture = texture(), .rate = 0.0F, .lifetime = {5.0F, 5.0F}, .speed = {10.0F, 10.0F}, .direction = 0.0F, .spread = 0.0F, .shape = EmitterConfig::Shape::Cone, .shapeSize = {8.0F, 0.0F}}, 2);
    cone.burst(20);
    for (const math::Vec2 point : cone.getPositions()) {
        EXPECT_GE(point.x, 0.0F);
        EXPECT_LE(point.x, 8.0F);
        EXPECT_NEAR(point.y, 0.0F, 1e-5F);
    }

    Emitter radial({.texture = texture(), .rate = 0.0F, .lifetime = {5.0F, 5.0F}, .speed = {0.0F, 0.0F}, .radialAcceleration = {100.0F, 100.0F}, .shape = EmitterConfig::Shape::Ring, .shapeSize = {10.0F, 0.0F}}, 3);
    radial.position = {50.0F, 50.0F};
    radial.burst(8);
    radial.update(0.1F);
    for (const math::Vec2 point : radial.getPositions()) {
        EXPECT_NEAR((point - radial.position).getLength(), 11.0F, 1e-3F);
    }

    Emitter swirl({.texture = texture(), .rate = 0.0F, .lifetime = {5.0F, 5.0F}, .speed = {0.0F, 0.0F}, .tangentialAcceleration = {100.0F, 100.0F}, .shape = EmitterConfig::Shape::Ring, .shapeSize = {10.0F, 0.0F}, .localSpace = true}, 4);
    swirl.burst(1);
    const math::Vec2 before = swirl.getPositions().front();
    swirl.update(0.1F);
    const math::Vec2 after = swirl.getPositions().front();
    EXPECT_GT(before.x * after.y - before.y * after.x, 0.0F);
}

TEST_F(ParticlesTest, UpdatesLargeEmittersInParallel) {
    const EmitterConfig config{.texture = texture(), .rate = 0.0F, .maxParticles = 20000, .lifetime = {0.5F, 2.0F}, .gravity = {0.0F, 30.0F}, .radialAcceleration = {-20.0F, 20.0F}, .tangentialAcceleration = {5.0F, 15.0F}, .damping = 0.3F, .turbulence = {.strength = 40.0F}, .attractors = {{.position = {30.0F, 0.0F}, .strength = 50.0F, .radius = 60.0F, .killRadius = 2.0F}}, .collision = {.type = EmitterConfig::Collision::Type::Floor, .y = 30.0F}, .shape = EmitterConfig::Shape::Circle, .shapeSize = {40.0F, 0.0F}};
    Emitter serial(config, 9);
    Emitter parallel(config, 9);
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

TEST_F(ParticlesTest, ColorsFollowTheirStopsStepsAndTints) {
    const math::Color red{1.0F, 0.0F, 0.0F, 1.0F};
    const math::Color green{0.0F, 1.0F, 0.0F, 1.0F};
    const math::Color blue{0.0F, 0.0F, 1.0F, 1.0F};
    Emitter stops({.texture = texture(), .rate = 0.0F, .lifetime = {1.0F, 1.0F}, .speed = {0.0F, 0.0F}, .colors = {red, green, blue}, .colorTimes = {0.0F, 0.2F, 1.0F}});
    stops.burst(1);
    stops.update(0.1F);
    EXPECT_NEAR(stops.getParticle(0).sprite.color.r, 0.5F, 1e-4F);
    stops.update(0.5F);
    EXPECT_NEAR(stops.getParticle(0).sprite.color.b, 0.5F, 1e-4F);

    Emitter steps({.texture = texture(), .rate = 0.0F, .lifetime = {1.0F, 1.0F}, .speed = {0.0F, 0.0F}, .colors = {red, green, blue}, .colorBlend = EmitterConfig::ColorBlend::Steps});
    steps.burst(1);
    steps.update(0.5F);
    EXPECT_EQ(steps.getParticle(0).sprite.color, green);
    steps.update(0.4F);
    EXPECT_EQ(steps.getParticle(0).sprite.color, blue);

    // Tints multiply the ramp, picked in turn, at random or by the moment of the cycle.
    Emitter cycle({.texture = texture(), .rate = 0.0F, .lifetime = {1.0F, 1.0F}, .tints = {red, blue}, .tintMode = EmitterConfig::TintMode::Cycle});
    cycle.burst(3);
    EXPECT_EQ(cycle.getParticle(0).sprite.color, red);
    EXPECT_EQ(cycle.getParticle(1).sprite.color, blue);
    EXPECT_EQ(cycle.getParticle(2).sprite.color, red);
    Emitter random({.texture = texture(), .rate = 0.0F, .lifetime = {1.0F, 1.0F}, .tints = {red, blue}}, 5);
    random.burst(40);
    const auto reds = std::count_if(random.getPositions().begin(), random.getPositions().end(), [&random, &red, index = std::size_t{0}](math::Vec2) mutable { return random.getParticle(index++).sprite.color == red; });
    EXPECT_GT(reds, 5);
    EXPECT_LT(reds, 35);
    Emitter timed({.texture = texture(), .rate = 10.0F, .duration = 1.0F, .lifetime = {5.0F, 5.0F}, .tints = {red, green}, .tintMode = EmitterConfig::TintMode::CycleTime});
    timed.update(0.3F);
    timed.update(0.4F);
    EXPECT_EQ(timed.getParticle(0).sprite.color, red);
    EXPECT_EQ(timed.getParticle(timed.getCount() - 1).sprite.color, green);

    EXPECT_THROW(Emitter({.texture = texture(), .colors = {red, green}, .colorTimes = {0.0F}}), std::invalid_argument);
    EXPECT_THROW(Emitter({.texture = texture(), .colors = {red, green}, .colorTimes = {0.6F, 0.4F}}), std::invalid_argument);
}

TEST_F(ParticlesTest, SizesSpeedsAndSpinsFollowTheirCurves) {
    Emitter popping({.texture = texture(), .rate = 0.0F, .lifetime = {1.0F, 1.0F}, .speed = {0.0F, 0.0F}, .startSize = {0.0F, 0.0F}, .endSize = {10.0F, 10.0F}, .sizeCurve = math::EasingCurve::points({{0.0F, 0.0F}, {0.5F, 1.5F}, {1.0F, 1.0F}})});
    popping.burst(1);
    popping.update(0.5F);
    EXPECT_NEAR(popping.getParticle(0).sprite.size.x, 15.0F, 1e-3F);

    // A scale of the end size keeps the size of each particle in proportion to its own start size.
    Emitter constant({.texture = texture(), .rate = 0.0F, .lifetime = {1.0F, 1.0F}, .startSize = {4.0F, 20.0F}, .endSize = {100.0F, 100.0F}, .endSizeScale = math::FloatRange{1.0F, 1.0F}}, 8);
    constant.burst(10);
    std::vector<float> sizes;
    for (std::size_t index = 0; index < 10; ++index) {
        sizes.push_back(constant.getParticle(index).sprite.size.x);
    }
    constant.update(0.7F);
    for (std::size_t index = 0; index < 10; ++index) {
        EXPECT_FLOAT_EQ(constant.getParticle(index).sprite.size.x, sizes[index]);
    }

    // Speed and spin curves scale the motion and the turning across the life, so a step curve stops both at half the life.
    Emitter stopping({.texture = texture(), .rate = 0.0F, .lifetime = {2.0F, 2.0F}, .speed = {100.0F, 100.0F}, .speedCurve = math::EasingCurve::points({{0.0F, 1.0F}, {0.5F, 1.0F}, {0.5001F, 0.0F}, {1.0F, 0.0F}}), .direction = 0.0F, .spread = 0.0F, .spin = {2.0F, 2.0F}, .spinCurve = math::EasingCurve::points({{0.0F, 1.0F}, {0.5F, 1.0F}, {0.5001F, 0.0F}, {1.0F, 0.0F}})});
    stopping.burst(1);
    for (int step = 0; step < 10; ++step) {
        stopping.update(0.1F);
    }
    const float x = stopping.getParticle(0).position.x;
    const float turned = stopping.getParticle(0).sprite.rotation;
    stopping.update(0.5F);
    EXPECT_NEAR(x, 100.0F, 1.0F);
    EXPECT_FLOAT_EQ(stopping.getParticle(0).position.x, x);
    EXPECT_FLOAT_EQ(stopping.getParticle(0).sprite.rotation, turned);
}

TEST_F(ParticlesTest, ParticlesTurnStretchAndSnap) {
    // Rotations start in their range and snap to quarter turns, and aligned particles follow their motion.
    Emitter stepped({.texture = texture(), .rate = 0.0F, .rotation = {0.3F, 1.2F}, .rotationStep = 1.5707964F}, 2);
    stepped.burst(20);
    for (std::size_t index = 0; index < 20; ++index) {
        const float rotation = stepped.getParticle(index).sprite.rotation;
        EXPECT_TRUE(std::fabs(rotation) < 1e-5F || std::fabs(rotation - 1.5707964F) < 1e-5F);
    }

    Emitter aligned({.texture = texture(), .rate = 0.0F, .speed = {100.0F, 100.0F}, .direction = 1.5707964F, .spread = 0.0F, .startSize = {10.0F, 10.0F}, .endSize = {10.0F, 10.0F}, .aspect = {2.0F, 2.0F}, .stretch = 0.1F, .alignToVelocity = true});
    aligned.burst(1);
    const graphics2d::SpriteInstance sprite = aligned.getParticle(0).sprite;
    EXPECT_NEAR(sprite.rotation, 1.5707964F, 1e-5F);
    EXPECT_NEAR(sprite.size.x, 30.0F, 1e-4F);
    EXPECT_NEAR(sprite.size.y, 10.0F, 1e-4F);

    Emitter snapped({.texture = texture(), .rate = 0.0F, .speed = {0.0F, 0.0F}, .shape = EmitterConfig::Shape::Rectangle, .shapeSize = {50.0F, 50.0F}, .pixelSnap = 4.0F}, 3);
    snapped.burst(10);
    for (std::size_t index = 0; index < 10; ++index) {
        const math::Vec2 at = snapped.getParticle(index).sprite.position;
        EXPECT_FLOAT_EQ(std::fmod(std::fabs(at.x), 4.0F), 0.0F);
        EXPECT_FLOAT_EQ(std::fmod(std::fabs(at.y), 4.0F), 0.0F);
    }
    EXPECT_THROW(Emitter({.texture = texture(), .aspect = {0.0F, 1.0F}}), std::invalid_argument);
    EXPECT_THROW(Emitter({.texture = texture(), .pixelSnap = -1.0F}), std::invalid_argument);
}

TEST_F(ParticlesTest, FramesPlayInEveryMode) {
    const graphics::Texture sheet = texture(64, 32);
    Emitter grid({.texture = sheet, .frameGrid = {.columns = 4, .rows = 2, .count = 6}, .rate = 0.0F, .lifetime = {1.0F, 1.0F}});
    grid.burst(1);
    grid.update(0.5F);
    EXPECT_EQ(grid.getParticle(0).sprite.source, (math::Rect{48.0F, 0.0F, 16.0F, 16.0F}));
    grid.update(0.4F);
    EXPECT_EQ(grid.getParticle(0).sprite.source, (math::Rect{16.0F, 16.0F, 16.0F, 16.0F}));

    Emitter looping({.texture = sheet, .frameGrid = {.columns = 4, .rows = 1}, .frameMode = EmitterConfig::FrameMode::Loop, .frameRate = 10.0F, .rate = 0.0F, .lifetime = {5.0F, 5.0F}});
    looping.burst(1);
    looping.update(0.55F);
    EXPECT_EQ(looping.getParticle(0).sprite.source.x, 16.0F);

    // A random frame stays for the whole life, and random starts spread the particles over the frames.
    Emitter random({.texture = sheet, .frameGrid = {.columns = 4, .rows = 1}, .frameMode = EmitterConfig::FrameMode::Random, .rate = 0.0F, .lifetime = {5.0F, 5.0F}}, 4);
    random.burst(30);
    std::vector<float> first;
    for (std::size_t index = 0; index < 30; ++index) {
        first.push_back(random.getParticle(index).sprite.source.x);
    }
    random.update(1.0F);
    for (std::size_t index = 0; index < 30; ++index) {
        EXPECT_EQ(random.getParticle(index).sprite.source.x, first[index]);
    }
    EXPECT_GT(std::count(first.begin(), first.end(), 0.0F), 0);
    EXPECT_LT(std::count(first.begin(), first.end(), 0.0F), 30);

    EXPECT_THROW(Emitter({.texture = sheet, .frames = {{0.0F, 0.0F, 4.0F, 4.0F}}, .frameGrid = {.columns = 2, .rows = 1}}), std::invalid_argument);
    EXPECT_THROW(Emitter({.texture = sheet, .frameGrid = {.columns = 2, .rows = 1, .count = 3}}), std::invalid_argument);
    EXPECT_THROW(Emitter({.texture = sheet, .frameRate = 0.0F}), std::invalid_argument);
}

TEST_F(ParticlesTest, TurbulenceAndAttractorsMoveParticles) {
    Emitter calm({.texture = texture(), .rate = 0.0F, .lifetime = {5.0F, 5.0F}, .speed = {0.0F, 0.0F}, .shape = EmitterConfig::Shape::Rectangle, .shapeSize = {200.0F, 200.0F}}, 6);
    Emitter stirred({.texture = texture(), .rate = 0.0F, .lifetime = {5.0F, 5.0F}, .speed = {0.0F, 0.0F}, .turbulence = {.strength = 300.0F, .frequency = 0.02F}, .shape = EmitterConfig::Shape::Rectangle, .shapeSize = {200.0F, 200.0F}}, 6);
    calm.burst(20);
    stirred.burst(20);
    calm.update(0.5F);
    stirred.update(0.5F);
    float moved = 0.0F;
    for (std::size_t index = 0; index < 20; ++index) {
        moved += math::Vec2::distance(calm.getPositions()[index], stirred.getPositions()[index]);
    }
    EXPECT_GT(moved, 20.0F);

    // Attractors pull particles in and remove the ones that reach the kill radius, and the app can move them.
    Emitter pulled({.texture = texture(), .rate = 0.0F, .lifetime = {10.0F, 10.0F}, .speed = {0.0F, 0.0F}, .attractors = {{.position = {100.0F, 0.0F}, .strength = 2000.0F, .radius = 200.0F, .killRadius = 5.0F}}});
    pulled.burst(1);
    pulled.update(0.1F);
    EXPECT_GT(pulled.getPositions().front().x, 0.0F);
    for (int step = 0; step < 60 && pulled.getCount() > 0; ++step) {
        pulled.update(1.0F / 30.0F);
    }
    EXPECT_EQ(pulled.getCount(), 0U);

    Emitter world({.texture = texture(), .rate = 0.0F, .lifetime = {10.0F, 10.0F}, .speed = {0.0F, 0.0F}, .attractors = {{.strength = 1000.0F, .radius = 1000.0F, .space = EmitterConfig::Attractor::Space::World}}});
    world.position = {500.0F, 500.0F};
    world.setAttractor(0, {500.0F, 0.0F});
    world.burst(1);
    world.update(0.1F);
    EXPECT_LT(world.getPositions().front().y, 500.0F);
    EXPECT_FLOAT_EQ(world.getPositions().front().x, 500.0F);
    EXPECT_THROW(world.setAttractor(1, {}), std::out_of_range);
}

TEST_F(ParticlesTest, ParticlesCollideWithFloorsBoundsAndWorlds) {
    using Collision = EmitterConfig::Collision;
    Emitter bouncing({.texture = texture(), .rate = 0.0F, .lifetime = {10.0F, 10.0F}, .speed = {200.0F, 200.0F}, .direction = 1.5707964F, .spread = 0.0F, .collision = {.type = Collision::Type::Floor, .y = 10.0F, .bounce = 0.5F}});
    bouncing.burst(1);
    bouncing.update(0.1F);
    EXPECT_FLOAT_EQ(bouncing.getPositions().front().y, 10.0F);
    EXPECT_NEAR(bouncing.getParticle(0).velocity.y, -100.0F, 1e-3F);

    Emitter sticking({.texture = texture(), .rate = 0.0F, .lifetime = {10.0F, 10.0F}, .speed = {200.0F, 200.0F}, .direction = 1.5707964F, .spread = 0.0F, .gravity = {0.0F, 500.0F}, .collision = {.type = Collision::Type::Floor, .y = 10.0F, .result = Collision::Result::Stick}});
    sticking.burst(1);
    sticking.update(0.1F);
    sticking.update(0.5F);
    EXPECT_FLOAT_EQ(sticking.getPositions().front().y, 10.0F);
    EXPECT_TRUE(sticking.getParticle(0).velocity.isZero());

    Emitter dying({.texture = texture(), .rate = 0.0F, .lifetime = {10.0F, 10.0F}, .speed = {200.0F, 200.0F}, .direction = 0.0F, .spread = 0.0F, .collision = {.type = Collision::Type::Bounds, .area = {-50.0F, -50.0F, 100.0F, 100.0F}, .result = Collision::Result::Die}});
    dying.burst(1);
    dying.update(0.2F);
    dying.update(0.2F);
    EXPECT_EQ(dying.getCount(), 0U);

    // World collision casts each move through the shapes of a physics world.
    physics2d::World world;
    physics2d::Body wall = world.createBody({.type = physics2d::Body::Type::Static, .position = {100.0F, 0.0F}});
    wall.addBox({20.0F, 400.0F});
    Emitter hitting({.texture = texture(), .rate = 0.0F, .lifetime = {10.0F, 10.0F}, .speed = {400.0F, 400.0F}, .direction = 0.0F, .spread = 0.0F, .collision = {.type = Collision::Type::World, .bounce = 1.0F}});
    hitting.setCollisionWorld(&world);
    hitting.burst(1);
    for (int step = 0; step < 10; ++step) {
        hitting.update(0.05F);
    }
    EXPECT_LT(hitting.getPositions().front().x, 90.0F);
    EXPECT_LT(hitting.getParticle(0).velocity.x, 0.0F);
    hitting.setCollisionWorld(nullptr);

    EXPECT_THROW(Emitter({.texture = texture(), .collision = {.type = Collision::Type::Bounds}}), std::invalid_argument);
    EXPECT_THROW(Emitter({.texture = texture(), .collision = {.friction = 2.0F}}), std::invalid_argument);
}

TEST_F(ParticlesTest, BoundsKillOrWrapParticles) {
    Emitter killing({.texture = texture(), .rate = 0.0F, .lifetime = {10.0F, 10.0F}, .speed = {100.0F, 100.0F}, .direction = 0.0F, .spread = 0.0F, .bounds = math::Rect{-50.0F, -50.0F, 100.0F, 100.0F}});
    killing.burst(1);
    killing.update(0.4F);
    EXPECT_EQ(killing.getCount(), 1U);
    killing.update(0.2F);
    EXPECT_EQ(killing.getCount(), 0U);

    Emitter wrapping({.texture = texture(), .rate = 0.0F, .lifetime = {10.0F, 10.0F}, .speed = {100.0F, 100.0F}, .direction = 0.0F, .spread = 0.0F, .bounds = math::Rect{-50.0F, -50.0F, 100.0F, 100.0F}, .boundsMode = EmitterConfig::BoundsMode::Wrap, .particleOrder = EmitterConfig::ParticleOrder::NewestFirst});
    wrapping.burst(1);
    wrapping.update(0.6F);
    EXPECT_NEAR(wrapping.getPositions().front().x, -40.0F, 1e-3F);
    EXPECT_EQ(drawFrame({&wrapping}).sprites, 1U);
    EXPECT_THROW(Emitter({.texture = texture(), .bounds = math::Rect{0.0F, 0.0F, 0.0F, 10.0F}}), std::invalid_argument);
}

TEST_F(ParticlesTest, ShapesAndDirectionModesPlaceParticles) {
    using Shape = EmitterConfig::Shape;
    Emitter ellipse({.texture = texture(), .rate = 0.0F, .shape = Shape::Ellipse, .shapeSize = {40.0F, 10.0F}}, 1);
    ellipse.burst(50);
    for (const math::Vec2 point : ellipse.getPositions()) {
        EXPECT_LE((point.x / 40.0F) * (point.x / 40.0F) + (point.y / 10.0F) * (point.y / 10.0F), 1.0001F);
    }

    Emitter edge({.texture = texture(), .rate = 0.0F, .shape = Shape::RectangleEdge, .shapeSize = {30.0F, 20.0F}}, 2);
    edge.burst(50);
    for (const math::Vec2 point : edge.getPositions()) {
        EXPECT_TRUE(std::fabs(std::fabs(point.x) - 30.0F) < 1e-3F || std::fabs(std::fabs(point.y) - 20.0F) < 1e-3F);
    }

    // Arcs stay inside their angles, and outward particles leave away from the center.
    Emitter arc({.texture = texture(), .rate = 0.0F, .speed = {10.0F, 10.0F}, .directionMode = EmitterConfig::DirectionMode::Outward, .spread = 0.0F, .shape = Shape::Arc, .shapeSize = {50.0F, 0.0F}, .shapeArc = {0.0F, 1.0F}}, 3);
    arc.burst(30);
    for (std::size_t index = 0; index < 30; ++index) {
        const Emitter::Particle particle = arc.getParticle(index);
        EXPECT_NEAR(particle.position.getLength(), 50.0F, 1e-3F);
        EXPECT_GE(particle.position.getAngle(), -1e-4F);
        EXPECT_LE(particle.position.getAngle(), 1.0001F);
        EXPECT_NEAR(math::Vec2::dot(particle.velocity.getNormalized(), particle.position.getNormalized()), 1.0F, 1e-4F);
    }
    Emitter inward({.texture = texture(), .rate = 0.0F, .speed = {10.0F, 10.0F}, .directionMode = EmitterConfig::DirectionMode::Inward, .spread = 0.0F, .shape = Shape::Ring, .shapeSize = {50.0F, 0.0F}}, 4);
    Emitter tangent({.texture = texture(), .rate = 0.0F, .speed = {10.0F, 10.0F}, .directionMode = EmitterConfig::DirectionMode::Tangent, .spread = 0.0F, .shape = Shape::Ring, .shapeSize = {50.0F, 0.0F}}, 4);
    inward.burst(1);
    tangent.burst(1);
    EXPECT_NEAR(math::Vec2::dot(inward.getParticle(0).velocity.getNormalized(), inward.getParticle(0).position.getNormalized()), -1.0F, 1e-4F);
    EXPECT_NEAR(math::Vec2::dot(tangent.getParticle(0).velocity.getNormalized(), tangent.getParticle(0).position.getNormalized()), 0.0F, 1e-4F);

    // Polygons spawn inside their outline and polylines along it, and the app can replace their points.
    Emitter polygon({.texture = texture(), .rate = 0.0F, .shape = Shape::Polygon, .shapePoints = {{0.0F, 0.0F}, {100.0F, 0.0F}, {0.0F, 100.0F}}}, 5);
    polygon.burst(50);
    for (const math::Vec2 point : polygon.getPositions()) {
        EXPECT_GE(point.x, -1e-3F);
        EXPECT_GE(point.y, -1e-3F);
        EXPECT_LE(point.x + point.y, 100.001F);
    }
    Emitter polyline({.texture = texture(), .rate = 0.0F, .shape = Shape::Polyline, .shapePoints = {{0.0F, 0.0F}, {100.0F, 0.0F}}}, 6);
    polyline.setShapePoints({{0.0F, 10.0F}, {0.0F, 60.0F}});
    polyline.burst(20);
    for (const math::Vec2 point : polyline.getPositions()) {
        EXPECT_NEAR(point.x, 0.0F, 1e-4F);
        EXPECT_GE(point.y, 10.0F - 1e-3F);
        EXPECT_LE(point.y, 60.0F + 1e-3F);
    }

    // The shape angle and the emitter rotation turn the spawn area, and the scale grows it.
    Emitter turned({.texture = texture(), .rate = 0.0F, .shape = Shape::Rectangle, .shapeSize = {50.0F, 0.0F}, .shapeAngle = 1.5707964F}, 7);
    turned.scale = 2.0F;
    turned.burst(20);
    for (const math::Vec2 point : turned.getPositions()) {
        EXPECT_NEAR(point.x, 0.0F, 1e-3F);
        EXPECT_LE(std::fabs(point.y), 100.001F);
    }

    EXPECT_THROW(Emitter({.texture = texture(), .shape = Shape::Polygon, .shapePoints = {{0.0F, 0.0F}, {1.0F, 0.0F}}}), std::invalid_argument);
    EXPECT_THROW(Emitter({.texture = texture(), .shape = Shape::Polygon, .shapePoints = {{0.0F, 0.0F}, {1.0F, 0.0F}, {2.0F, 0.0F}}}), std::invalid_argument);
    EXPECT_THROW(polyline.setShapePoints({{0.0F, 0.0F}}), std::invalid_argument);
    EXPECT_THROW(Emitter({.texture = texture(), .shape = Shape::Image}), std::invalid_argument);
}

TEST_F(ParticlesTest, ImageShapesSpawnOnVisiblePixelsInTheirColors) {
    graphics::Image image(4, 2);
    image.setPixel(0, 0, math::Color{1.0F, 0.0F, 0.0F, 1.0F});
    image.setPixel(3, 1, math::Color{0.0F, 0.0F, 1.0F, 1.0F});
    image.setPixel(1, 1, math::Color{0.0F, 1.0F, 0.0F, 0.2F});
    const auto shape = std::make_shared<ImageShape>(image, ImageShape::Options{});
    ASSERT_EQ(shape->getPoints().size(), 2U);
    EXPECT_EQ(shape->getPoints().front(), math::Vec2(-1.5F, -0.5F));
    EXPECT_EQ(shape->getSize(), math::Vec2(4.0F, 2.0F));
    EXPECT_EQ(ImageShape(image, {.source = {2.0F, 0.0F, 2.0F, 2.0F}}).getPoints().size(), 1U);
    EXPECT_EQ(ImageShape(image, {.alphaThreshold = 0.1F}).getPoints().size(), 3U);
    EXPECT_THROW(ImageShape(image, {.source = {2.0F, 0.0F, 4.0F, 2.0F}}), std::invalid_argument);
    EXPECT_THROW(ImageShape(graphics::Image(2, 2), {}), std::invalid_argument);

    Emitter burning({.texture = texture(), .rate = 0.0F, .speed = {0.0F, 0.0F}, .shape = EmitterConfig::Shape::Image, .shapeSize = {40.0F, 20.0F}, .shapeImage = shape, .colorFromImage = true}, 3);
    burning.burst(20);
    for (std::size_t index = 0; index < 20; ++index) {
        const Emitter::Particle particle = burning.getParticle(index);
        const bool left = particle.position.x < 0.0F;
        EXPECT_EQ(particle.sprite.color, left ? math::Color(1.0F, 0.0F, 0.0F, 1.0F) : math::Color(0.0F, 0.0F, 1.0F, 1.0F));
        EXPECT_NEAR(std::fabs(particle.position.x), 15.0F, 5.0001F);
    }
}

TEST_F(ParticlesTest, MovingEmittersInheritVelocityAndEmitOverDistance) {
    Emitter exhaust({.texture = texture(), .rate = 0.0F, .lifetime = {10.0F, 10.0F}, .speed = {0.0F, 0.0F}, .inheritVelocity = 0.5F});
    exhaust.update(0.1F);
    exhaust.position = {10.0F, 0.0F};
    exhaust.update(0.1F);
    exhaust.burst(1);
    EXPECT_NEAR(exhaust.getParticle(0).velocity.x, 50.0F, 1e-3F);

    // Distance emission spreads its particles along the move, so a fast trail leaves no gaps.
    Emitter trail({.texture = texture(), .rate = 0.0F, .rateOverDistance = 0.1F, .maxParticles = 100, .lifetime = {10.0F, 10.0F}, .speed = {0.0F, 0.0F}});
    trail.update(0.1F);
    trail.position = {100.0F, 0.0F};
    trail.update(0.1F);
    ASSERT_EQ(trail.getCount(), 10U);
    EXPECT_NEAR(trail.getPositions().front().x, 10.0F, 1e-3F);
    EXPECT_NEAR(trail.getPositions().back().x, 100.0F, 1e-3F);

    // The scale grows speeds and sizes, and the rotation turns the direction and the particles.
    Emitter turned({.texture = texture(), .rate = 0.0F, .speed = {10.0F, 10.0F}, .direction = 0.0F, .spread = 0.0F, .startSize = {4.0F, 4.0F}});
    turned.scale = 3.0F;
    turned.rotation = 1.5707964F;
    turned.burst(1);
    const Emitter::Particle particle = turned.getParticle(0);
    EXPECT_NEAR(particle.velocity.x, 0.0F, 1e-4F);
    EXPECT_NEAR(particle.velocity.y, 30.0F, 1e-4F);
    EXPECT_FLOAT_EQ(particle.sprite.size.x, 12.0F);
    EXPECT_NEAR(particle.sprite.rotation, 1.5707964F, 1e-5F);
}

TEST_F(ParticlesTest, DelaysAndRicherBurstsTimeTheEmission) {
    Emitter delayed({.texture = texture(), .rate = 10.0F, .delay = 0.5F, .lifetime = {10.0F, 10.0F}});
    delayed.update(0.3F);
    EXPECT_EQ(delayed.getCount(), 0U);
    EXPECT_TRUE(delayed.isAlive());
    delayed.update(0.4F);
    EXPECT_EQ(delayed.getCount(), 2U);

    // A burst repeats over its cycles, picks counts in its range and fires with its probability.
    Emitter crackle({.texture = texture(), .rate = 0.0F, .bursts = {{.time = 0.1F, .count = {2, 4}, .cycles = 3, .interval = 0.1F}}, .duration = 1.0F, .lifetime = {10.0F, 10.0F}}, 3);
    crackle.update(0.15F);
    const std::size_t first = crackle.getCount();
    EXPECT_GE(first, 2U);
    EXPECT_LE(first, 4U);
    crackle.update(0.2F);
    EXPECT_GE(crackle.getCount(), 6U);
    EXPECT_LE(crackle.getCount(), 12U);

    Emitter never({.texture = texture(), .rate = 0.0F, .bursts = {{.count = {5, 5}, .probability = 0.0F}}, .duration = 1.0F});
    never.update(0.5F);
    EXPECT_EQ(never.getCount(), 0U);

    EXPECT_THROW(Emitter({.texture = texture(), .bursts = {{.time = 0.5F, .count = {1, 1}, .cycles = 3, .interval = 0.3F}}, .duration = 1.0F}), std::invalid_argument);
    EXPECT_THROW(Emitter({.texture = texture(), .bursts = {{.count = {3, 1}}}}), std::invalid_argument);
    EXPECT_THROW(Emitter({.texture = texture(), .delay = -1.0F}), std::invalid_argument);
}

TEST_F(ParticlesTest, SubEmittersSpawnOnEveryTrigger) {
    using Trigger = EmitterConfig::SubEmitter::Trigger;
    const auto spark = std::make_shared<EmitterConfig>(EmitterConfig{.texture = texture(), .rate = 50.0F, .lifetime = {10.0F, 10.0F}, .speed = {0.0F, 0.0F}, .colors = {math::Color{0.5F, 0.5F, 0.5F, 1.0F}}, .localSpace = true});
    const math::Color red{1.0F, 0.0F, 0.0F, 1.0F};

    // Death spawns where the particle died, with the color it had.
    Emitter dying({.texture = texture(), .rate = 0.0F, .lifetime = {0.1F, 0.1F}, .speed = {0.0F, 0.0F}, .colors = {red}, .subEmitters = {{.config = spark, .trigger = Trigger::Death, .count = {3, 3}, .inheritColor = true}}});
    dying.position = {40.0F, 20.0F};
    dying.burst(2);
    dying.update(0.2F);
    EXPECT_EQ(dying.getCount(), 0U);
    EXPECT_TRUE(dying.isAlive());
    // clang-format off
    EXPECT_EQ(drawFrame({&dying}).sprites, 6U);
    // clang-format on

    // Birth spawns with each new particle, and the sub-emitter never emits on its own.
    Emitter born({.texture = texture(), .rate = 0.0F, .lifetime = {10.0F, 10.0F}, .subEmitters = {{.config = spark, .trigger = Trigger::Birth, .count = {2, 2}}}});
    born.burst(4);
    born.update(1.0F);
    EXPECT_EQ(drawFrame({&born}).sprites, 12U);

    // Hits and living particles spawn too, at a rate per particle while they live.
    Emitter hitting({.texture = texture(), .rate = 0.0F, .lifetime = {10.0F, 10.0F}, .speed = {200.0F, 200.0F}, .direction = 1.5707964F, .spread = 0.0F, .collision = {.type = EmitterConfig::Collision::Type::Floor, .y = 5.0F, .result = EmitterConfig::Collision::Result::Stick}, .subEmitters = {{.config = spark, .trigger = Trigger::Collision, .count = {1, 1}}}});
    hitting.burst(3);
    hitting.update(0.1F);
    hitting.update(0.1F);
    EXPECT_EQ(drawFrame({&hitting}).sprites, 6U);
    Emitter living({.texture = texture(), .rate = 0.0F, .lifetime = {10.0F, 10.0F}, .subEmitters = {{.config = spark, .trigger = Trigger::Alive, .rate = 20.0F}}}, 2);
    living.burst(1);
    for (int step = 0; step < 10; ++step) {
        living.update(0.1F);
    }
    EXPECT_NEAR(static_cast<double>(drawFrame({&living}).sprites), 21.0, 4.0);

    living.restart();
    EXPECT_EQ(drawFrame({&living}).sprites, 0U);
    EXPECT_THROW(Emitter({.texture = texture(), .subEmitters = {{.trigger = Trigger::Death}}}), std::invalid_argument);
}

TEST_F(ParticlesTest, LightsDrawOnlyInLitCanvases) {
    Emitter fire({.texture = texture(), .rate = 0.0F, .lifetime = {1.0F, 1.0F}, .light = EmitterConfig::Light{.radius = 200.0F, .flicker = {.speed = 8.0F, .amount = 0.2F}}, .particleLights = EmitterConfig::ParticleLights{.max = 4}});
    fire.burst(10);
    fire.update(0.1F);
    EXPECT_EQ(drawFrame({&fire}).lights, 0U);
    EXPECT_EQ(drawFrame({&fire}, math::Color::black()).lights, 5U);

    // A light that fades with the count goes out with the last particle.
    Emitter flash({.texture = texture(), .rate = 0.0F, .lifetime = {0.2F, 0.2F}, .light = EmitterConfig::Light{.fade = EmitterConfig::Light::Fade::Count}});
    flash.burst(1);
    flash.update(0.1F);
    EXPECT_EQ(drawFrame({&flash}, math::Color::black()).lights, 1U);
    flash.update(0.2F);
    EXPECT_EQ(drawFrame({&flash}, math::Color::black()).lights, 0U);
    EXPECT_THROW(Emitter({.texture = texture(), .light = EmitterConfig::Light{.radius = 0.0F}}), std::invalid_argument);
}

// Distortion particles bend the image of composited canvases, and plain canvases skip them.
TEST_F(ParticlesTest, DistortionParticlesBendOnlyCompositedCanvases) {
    Emitter haze({.texture = texture(), .rate = 0.0F, .lifetime = {1.0F, 1.0F}, .order = {.distortion = 0.6F}});
    haze.burst(5);
    EXPECT_EQ(drawFrame({&haze}).sprites, 0U);
    EXPECT_EQ(drawFrame({&haze}, math::Color::white()).sprites, 5U);
}

TEST_F(ParticlesTest, TrailsDrawRibbonsBehindParticlesAndPoints) {
    Emitter streaks({.texture = texture(), .rate = 0.0F, .lifetime = {10.0F, 10.0F}, .speed = {100.0F, 100.0F}, .trail = {.length = 4, .lifetime = 0.4F}});
    streaks.burst(2);
    for (int step = 0; step < 10; ++step) {
        streaks.update(0.1F);
    }
    // Every particle draws its sprite and a ribbon of five points, two vertices each.
    EXPECT_EQ(drawFrame({&streaks}).vertices, 20U);
    EXPECT_THROW(Emitter({.texture = texture(), .trail = {.length = 65}}), std::invalid_argument);

    Trail blade({.lifetime = 0.5F, .minDistance = 10.0F, .maxPoints = 4});
    blade.update(0.1F);
    blade.position = {5.0F, 0.0F};
    blade.update(0.1F);
    EXPECT_EQ(blade.getCount(), 1U);
    for (int step = 1; step <= 6; ++step) {
        blade.position = {static_cast<float>(step) * 20.0F, 0.0F};
        blade.update(0.05F);
    }
    EXPECT_EQ(blade.getCount(), 4U);
    blade.emitting = false;
    EXPECT_TRUE(blade.isAlive());
    blade.update(1.0F);
    EXPECT_EQ(blade.getCount(), 0U);
    EXPECT_FALSE(blade.isAlive());
    EXPECT_THROW(Trail({.lifetime = 0.0F}), std::invalid_argument);
    EXPECT_THROW(Trail({.maxPoints = 1}), std::invalid_argument);

    std::vector<graphics2d::MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
    const std::vector<math::Color> colors{math::Color::white(), math::Color::transparent()};
    const std::vector<math::Vec2> line{{0.0F, 0.0F}, {10.0F, 0.0F}, {20.0F, 0.0F}};
    const std::vector<float> places{0.0F, 0.5F, 1.0F};
    Ribbon::append(line, places, {.widthStart = 8.0F, .widthEnd = 0.0F, .colors = colors}, vertices, indices);
    ASSERT_EQ(vertices.size(), 6U);
    EXPECT_EQ(indices.size(), 12U);
    EXPECT_FLOAT_EQ(std::fabs(vertices[0].position.y), 4.0F);
    EXPECT_FLOAT_EQ(std::fabs(vertices[2].position.y), 2.0F);
    EXPECT_FLOAT_EQ(vertices[2].color.a, 0.5F);
}

TEST_F(ParticlesTest, SystemsMoveScaleAndRestartTheirEmitters) {
    Effect effect;
    effect.parts.push_back({.name = "core", .config = {.texture = texture(), .rate = 0.0F, .bursts = {{.count = {2, 2}}}, .duration = 0.1F, .speed = {0.0F, 0.0F}}});
    effect.parts.push_back({.name = "ring", .offset = {10.0F, 0.0F}, .scale = 2.0F, .config = {.texture = texture(), .rate = 0.0F, .bursts = {{.count = {3, 3}}}, .delay = 0.2F, .duration = 0.1F, .speed = {0.0F, 0.0F}}});
    System system(effect, 5);
    system.position = {100.0F, 50.0F};
    system.rotation = 1.5707964F;
    system.update(0.05F);
    EXPECT_EQ(system.getCount(), 2U);
    EXPECT_EQ(system.findEmitter("core")->getPositions().front(), math::Vec2(100.0F, 50.0F));
    system.update(0.3F);
    EXPECT_EQ(system.getCount(), 5U);
    const std::shared_ptr<Emitter> ring = system.findEmitter("ring");
    EXPECT_NEAR(ring->getPositions().front().x, 100.0F, 1e-3F);
    EXPECT_NEAR(ring->getPositions().front().y, 60.0F, 1e-3F);
    EXPECT_FLOAT_EQ(ring->scale, 2.0F);
    EXPECT_EQ(system.getEmitterCount(), 2U);
    EXPECT_EQ(system.getName(1), "ring");
    EXPECT_EQ(system.findEmitter("smoke"), nullptr);
    EXPECT_THROW((void)system.getEmitter(2), std::out_of_range);

    system.setEmitting(false);
    EXPECT_FALSE(system.isEmitting());
    system.restart();
    EXPECT_TRUE(system.isEmitting());
    EXPECT_EQ(system.getCount(), 0U);
    system.update(0.05F);
    system.clear();
    EXPECT_EQ(system.getCount(), 0U);
    EXPECT_TRUE(system.isAlive());
}

TEST(EffectTest, LoadsEffectFilesAsAssets) {
    const std::vector<std::uint8_t> png = test::TestFiles::pngImage(8, 8, 0xFFFFFFFFU);
    test::EngineFixture loaded({
        {"content/effects/sparks.particles", R"({
            "texture": "../images/spark.png", "frames": [[0, 0, 4, 4], [4, 0, 4, 4]], "rate": 0, "bursts": [{"time": 0, "count": 12}],
            "duration": 0.5, "loop": true, "prewarm": 0, "maxParticles": 64, "lifetime": [0.4, 0.6], "speed": 80, "direction": -1.57, "spread": 1,
            "gravity": [0, 200], "radialAcceleration": 10, "tangentialAcceleration": [0, 5], "damping": 0.1, "startSize": [4, 6], "endSize": 0,
            "spin": [-2, 2], "colors": ["#FFFFE080", "#00FF4000"], "shape": "cone", "shapeSize": [6, 0], "localSpace": true, "layer": 4, "depth": 0.5, "blend": "additive"
        })"},
        {"content/effects/everything.particles", R"({
            "texture": "../images/spark.png", "filter": "linear", "wrap": "repeat", "frameGrid": {"columns": 2, "rows": 2, "count": 3}, "frameMode": "loopRandomStart", "frameRate": 12,
            "rateOverDistance": 0.5, "bursts": [{"time": 0.1, "count": [2, 5], "cycles": 3, "interval": 0.1, "probability": 0.5}], "delay": 0.25, "duration": 1,
            "speedCurve": "quadOut", "directionMode": "outward", "inheritVelocity": 0.3, "turbulence": {"strength": 30, "frequency": 0.02, "speed": 1},
            "attractors": [{"x": 0, "y": -50, "strength": 300, "radius": 100, "killRadius": 4, "space": "world"}],
            "collision": {"type": "floor", "y": 40, "bounce": 0.3, "friction": 0.2, "result": "bounce", "lifeLoss": 0.1}, "bounds": [-100, -100, 200, 200], "boundsMode": "wrap",
            "endSizeScale": [0.5, 1], "sizeCurve": {"curve": "backOut", "overshoot": 3}, "aspect": [1, 2], "stretch": 0.02, "rotation": [0, 6.2832], "rotationStep": 1.5708,
            "alignToVelocity": true, "spinCurve": {"steps": 4}, "colors": ["#FFFFFFFF", "#80FF8000", "#00FF0000"], "colorTimes": [0, 0.3, 1], "colorBlend": "steps",
            "tints": ["#FFFF0000", "#FF0000FF"], "tintMode": "cycle", "shape": "arc", "shapeSize": [30, 0], "shapeAngle": 0.5, "shapeArc": [0, 3.14], "shapeThickness": 4,
            "pixelSnap": 2, "particleOrder": "newestFirst", "trail": {"length": 6, "lifetime": 0.2, "widthStart": 4, "widthEnd": 0, "colors": ["#FFFFFFFF", "#00FFFFFF"], "texture": "../images/line.png"},
            "light": {"type": "point", "offset": [0, -10], "radius": 120, "color": "#FFFFB060", "intensity": 1.5, "flicker": {"speed": 8, "amount": 0.2}, "fade": "cycle"},
            "particleLights": {"radius": 20, "intensity": 0.4, "max": 8}, "sortOffset": 3, "visibility": 2, "emission": 1, "unshaded": true, "lightMask": 3, "distortion": 0.25,
            "subEmitters": [{"effect": "sparks.particles", "trigger": "collision", "count": [1, 2], "probability": 0.5, "inheritVelocity": 0.2, "inheritColor": true, "overrides": {"localSpace": false, "texture": "../images/line.png"}}]
        })"},
        {"content/effects/outline.particles", R"({"texture": "../images/spark.png", "shape": "image", "shapeImage": {"path": "../images/spark.png", "source": [0, 0, 4, 4]}, "colorFromImage": true, "shapePoints": [[0, 0], [1, 1]]})"},
        {"content/effects/explosion.particles", R"({"emitters": [
            {"name": "flash", "effect": "sparks.particles", "offset": [0, -10], "scale": 2, "delay": 0.1, "layerOffset": 2, "overrides": {"rate": 5}},
            {"name": "smoke", "texture": "../images/spark.png", "rate": 3, "delay": 0.2}
        ]})"},
        {"content/effects/loop.particles", R"({"texture": "../images/spark.png", "subEmitters": [{"effect": "loop.particles"}]})"},
        {"content/effects/nested.particles", R"({"texture": "../images/spark.png", "subEmitters": [{"effect": "explosion.particles"}]})"},
        {"content/effects/broken.particles", R"({"texture": "../images/spark.png", "sped": 3})"},
        {"content/effects/negative.particles", R"({"texture": "../images/spark.png", "bursts": [{"time": 0, "count": -1}]})"},
        {"content/effects/unbounded.particles", R"({"texture": "../images/spark.png", "maxParticles": -1})"},
        {"content/effects/mode.particles", R"({"texture": "../images/spark.png", "boundsMode": "bounce"})"},
        {"content/effects/curve.particles", R"({"texture": "../images/spark.png", "sizeCurve": "wobble"})"},
        {"content/effects/nested-key.particles", R"({"texture": "../images/spark.png", "turbulence": {"strenght": 3}})"},
        {"content/effects/textureless.particles", R"({"rate": 3})"},
        {"content/images/spark.png", std::string(png.begin(), png.end())},
        {"content/images/line.png", std::string(png.begin(), png.end())},
    });
    assets::Manager& assets = loaded.engine().getAssets();
    const auto effect = std::static_pointer_cast<Effect>(assets.load("particles", "effects/sparks.particles"));
    EXPECT_EQ(effect->files.texture, "images/spark.png");
    EXPECT_EQ(effect->config.texture, assets.texture("images/spark.png"));
    EXPECT_EQ(effect->config.bursts.front().count.max, 12U);
    EXPECT_TRUE(effect->config.loop);
    EXPECT_EQ(effect->config.shape, EmitterConfig::Shape::Cone);
    EXPECT_EQ(effect->config.tangentialAcceleration.max, 5.0F);
    EXPECT_EQ(effect->config.colors.back(), math::Color::parse("#00FF4000"));
    EXPECT_EQ(effect->config.order.blend, graphics::BlendMode::Type::Additive);
    EXPECT_EQ(effect->config.order.layer, 4);
    EXPECT_EQ(assets.getTypeForPath("effects/sparks.particles"), "particles");

    // Every key of the format reads into the configuration, and the file filter applies unless the caller names another one.
    const auto everything = std::static_pointer_cast<Effect>(assets.load("particles", "effects/everything.particles"));
    const EmitterConfig& config = everything->config;
    EXPECT_EQ(config.texture, assets.texture("images/spark.png", {.filter = graphics::Texture::Filter::Linear, .wrap = graphics::Texture::Wrap::Repeat}));
    EXPECT_EQ(config.frameGrid.count, 3);
    EXPECT_EQ(config.frameMode, EmitterConfig::FrameMode::LoopRandomStart);
    EXPECT_EQ(config.bursts.front().cycles, 3);
    EXPECT_EQ(config.bursts.front().count.min, 2U);
    EXPECT_FLOAT_EQ(config.bursts.front().probability, 0.5F);
    EXPECT_FLOAT_EQ(config.delay, 0.25F);
    EXPECT_EQ(config.speedCurve.getType(), math::Easing::Type::QuadOut);
    EXPECT_EQ(config.directionMode, EmitterConfig::DirectionMode::Outward);
    EXPECT_FLOAT_EQ(config.turbulence.strength, 30.0F);
    EXPECT_EQ(config.attractors.front().space, EmitterConfig::Attractor::Space::World);
    EXPECT_EQ(config.collision.type, EmitterConfig::Collision::Type::Floor);
    EXPECT_FLOAT_EQ(config.collision.lifeLoss, 0.1F);
    EXPECT_EQ(config.boundsMode, EmitterConfig::BoundsMode::Wrap);
    EXPECT_EQ(config.sizeCurve.getKind(), math::EasingCurve::Kind::Parametric);
    EXPECT_EQ(config.spinCurve.getKind(), math::EasingCurve::Kind::Steps);
    EXPECT_EQ(config.colorBlend, EmitterConfig::ColorBlend::Steps);
    EXPECT_EQ(config.tintMode, EmitterConfig::TintMode::Cycle);
    EXPECT_EQ(config.shape, EmitterConfig::Shape::Arc);
    EXPECT_FLOAT_EQ(config.shapeArc.max, 3.14F);
    EXPECT_EQ(config.particleOrder, EmitterConfig::ParticleOrder::NewestFirst);
    EXPECT_EQ(config.trail.texture, assets.texture("images/line.png", {.filter = graphics::Texture::Filter::Linear, .wrap = graphics::Texture::Wrap::Repeat}));
    ASSERT_TRUE(config.light.has_value());
    EXPECT_FLOAT_EQ(config.light->flicker.amount, 0.2F);
    EXPECT_EQ(config.light->fade, EmitterConfig::Light::Fade::Cycle);
    EXPECT_EQ(config.particleLights->max, 8U);
    EXPECT_TRUE(config.order.unshaded);
    EXPECT_EQ(config.order.lightMask, 3);
    EXPECT_FLOAT_EQ(config.order.distortion, 0.25F);
    ASSERT_EQ(config.subEmitters.size(), 1U);
    EXPECT_EQ(config.subEmitters.front().trigger, EmitterConfig::SubEmitter::Trigger::Collision);
    EXPECT_FALSE(config.subEmitters.front().config->localSpace);
    EXPECT_EQ(config.subEmitters.front().config->texture, assets.texture("images/line.png"));
    EXPECT_NO_THROW(Emitter(config, 1).update(0.5F));
    const auto nearest = std::static_pointer_cast<Effect>(assets.load("particles", "effects/everything.particles", {{"filter", "nearest"}}));
    EXPECT_EQ(nearest->config.texture, assets.texture("images/spark.png", {.filter = graphics::Texture::Filter::Nearest, .wrap = graphics::Texture::Wrap::Repeat}));

    const auto outline = std::static_pointer_cast<Effect>(assets.load("particles", "effects/outline.particles"));
    EXPECT_EQ(outline->config.shapeImage->getPoints().size(), 16U);

    // A composite effect lists its emitters, each another file with overrides or the options themselves.
    const auto explosion = std::static_pointer_cast<Effect>(assets.load("particles", "effects/explosion.particles"));
    ASSERT_TRUE(explosion->isComposite());
    ASSERT_EQ(explosion->parts.size(), 2U);
    EXPECT_EQ(explosion->parts[0].name, "flash");
    EXPECT_EQ(explosion->parts[0].offset, math::Vec2(0.0F, -10.0F));
    EXPECT_FLOAT_EQ(explosion->parts[0].scale, 2.0F);
    EXPECT_FLOAT_EQ(explosion->parts[0].config.rate, 5.0F);
    EXPECT_FLOAT_EQ(explosion->parts[0].config.delay, 0.1F);
    EXPECT_EQ(explosion->parts[0].config.order.layer, 6);
    EXPECT_EQ(explosion->parts[1].config.texture, assets.texture("images/spark.png"));
    EXPECT_EQ(System(*explosion).getEmitterCount(), 2U);

    // clang-format off
    const auto failure = [&assets](const std::string& path) {
        try {
            (void)assets.load("particles", path);
        } catch (const std::exception& error) {
            return std::string(error.what());
        }
        return std::string("no error");
    };
    // clang-format on
    EXPECT_EQ(failure("effects/broken.particles"), "The particle effect \"effects/broken.particles\" has the unknown option \"sped\".");
    EXPECT_EQ(failure("effects/negative.particles"), "The particle effect value \"count\" needs an integer of at least 0.");
    EXPECT_EQ(failure("effects/unbounded.particles"), "The particle effect value \"maxParticles\" needs an integer of at least 0.");
    EXPECT_EQ(failure("effects/mode.particles"), "The particle effect value \"boundsMode\" must be \"kill\" or \"wrap\", not \"bounce\".");
    EXPECT_EQ(failure("effects/curve.particles"), "The particle effect value \"sizeCurve\" names the unknown curve \"wobble\".");
    EXPECT_EQ(failure("effects/nested-key.particles"), "The particle effect value \"turbulence\" has the unknown option \"strenght\".");
    EXPECT_EQ(failure("effects/textureless.particles"), "The particle effect \"effects/textureless.particles\" needs a texture.");
    EXPECT_EQ(failure("effects/loop.particles"), "The particle effect \"effects/loop.particles\" refers to itself through \"effects/loop.particles\".");
    EXPECT_EQ(failure("effects/nested.particles"), "The particle effect \"effects/nested.particles\" refers to the composite effect \"effects/explosion.particles\", where it needs an effect of one emitter.");
}

} // namespace haylen::particles2d
