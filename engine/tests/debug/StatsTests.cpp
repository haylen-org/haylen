#include <gtest/gtest.h>

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "graphics/Gpu.hpp"
#include "haylen/2d/particles/Emitter.hpp"
#include "haylen/2d/physics/World.hpp"
#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/debug/ObjectCounter.hpp"
#include "haylen/debug/Stats.hpp"
#include "haylen/debug/TrackedCount.hpp"
#include "haylen/debug/TrackedObject.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/plugins/DebugPlugin.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::debug {

namespace {

// Reads a counter the way the statistics do, as zero before its first object.
ObjectCounter::Snapshot counted(std::string_view name) {
    return ObjectCounter::find(name).value_or(ObjectCounter::Snapshot{});
}

graphics::Device::Pool pool(graphics::Device& device, std::string_view name) {
    for (const graphics::Device::Pool& found : device.getPools()) {
        if (found.name == name) {
            return found;
        }
    }
    return {};
}

} // namespace

TEST(ObjectCounterTest, CountsTrackedObjectsAndTheirMemory) {
    ObjectCounter counter("StatsTests.Thing", ObjectCounter::Kind::Native);
    {
        TrackedObject first(counter);
        first.setBytes(100);
        const TrackedObject copy(first);
        TrackedObject assigned(counter);
        assigned = first;
        EXPECT_EQ(counted("StatsTests.Thing").alive, 3U);
        EXPECT_EQ(counted("StatsTests.Thing").bytes, 100);
        first.setBytes(40);
        EXPECT_EQ(counted("StatsTests.Thing").bytes, 40);
    }
    const ObjectCounter::Snapshot after = counted("StatsTests.Thing");
    EXPECT_EQ(after.created, 3U);
    EXPECT_EQ(after.destroyed, 3U);
    EXPECT_EQ(after.alive, 0U);
    EXPECT_EQ(after.bytes, 0);

    {
        TrackedCount held(counter);
        held.set(10);
        held.set(4);
        const TrackedCount copy(held);
        EXPECT_EQ(counted("StatsTests.Thing").alive, 8U);
    }
    EXPECT_EQ(counted("StatsTests.Thing").created, 17U);
    EXPECT_EQ(counted("StatsTests.Thing").alive, 0U);
    EXPECT_FALSE(ObjectCounter::find("StatsTests.Missing").has_value());

    std::vector<std::string> seen;
    // clang-format off
    ObjectCounter::setObserver([&seen](std::string_view name, bool created, std::size_t count) {
        if (name == "StatsTests.Thing") {
            seen.push_back((created ? "+" : "-") + std::to_string(count));
        }
    });
    // clang-format on
    EXPECT_TRUE(ObjectCounter::isObserved());
    { const TrackedObject observed(counter); }
    ObjectCounter::setObserver({});
    EXPECT_FALSE(ObjectCounter::isObserved());
    { const TrackedObject unobserved(counter); }
    EXPECT_EQ(seen, (std::vector<std::string>{"+1", "-1"}));
}

TEST(ObjectCounterTest, CountsLuaUserdataUntilTheyAreCollected) {
    test::EngineFixture fixture;
    const ObjectCounter::Snapshot before = counted("haylen.Signal");
    fixture.runLua("signals = {} for index = 1, 5 do signals[index] = require('haylen.signal').new() end");
    EXPECT_EQ(counted("haylen.Signal").created, before.created + 5);
    EXPECT_EQ(counted("haylen.Signal").alive, before.alive + 5);
    EXPECT_EQ(counted("haylen.Signal").kind, ObjectCounter::Kind::Userdata);

    fixture.runLua("signals = nil collectgarbage() collectgarbage()");
    EXPECT_EQ(counted("haylen.Signal").destroyed, before.destroyed + 5);
    EXPECT_EQ(counted("haylen.Signal").alive, before.alive);
}

TEST(ObjectCounterTest, CountsEngineResources) {
    test::EngineFixture fixture;
    graphics::Device& device = fixture.engine().getGraphics();
    const ObjectCounter::Snapshot textures = counted("Texture");
    const ObjectCounter::Snapshot targets = counted("RenderTarget");
    const graphics::Device::Pool images = pool(device, "images");
    EXPECT_EQ(images.size, graphics::Gpu::kImagePoolSize);
    {
        const graphics::Texture texture = device.createTexture(graphics::Image(8, 4, math::Color::white()));
        const graphics::RenderTarget target = device.createRenderTarget(16, 16);
        EXPECT_EQ(counted("Texture").alive, textures.alive + 1);
        EXPECT_EQ(counted("Texture").bytes, textures.bytes + 8 * 4 * 4);
        EXPECT_EQ(counted("RenderTarget").alive, targets.alive + 1);
        EXPECT_EQ(counted("RenderTarget").bytes, targets.bytes + 16 * 16 * 4);
        EXPECT_EQ(pool(device, "images").used, images.used + 2);
        EXPECT_GE(pool(device, "views").used, pool(device, "images").used);
    }
    device.collectGarbage();
    EXPECT_EQ(counted("Texture").alive, textures.alive);
    EXPECT_EQ(counted("RenderTarget").bytes, targets.bytes);
    EXPECT_EQ(pool(device, "images").used, images.used);

    // Bodies and contacts follow the world, and particles follow their emitter.
    const std::uint64_t bodies = counted("PhysicsBody").alive;
    {
        physics2d::World world;
        physics2d::Body body = world.createBody();
        world.createBody();
        EXPECT_EQ(counted("PhysicsBody").alive, bodies + 2);
        body.destroy();
        EXPECT_EQ(counted("PhysicsBody").alive, bodies + 1);
        EXPECT_EQ(counted("PhysicsBody").destroyed, counted("PhysicsBody").created - counted("PhysicsBody").alive);
    }
    EXPECT_EQ(counted("PhysicsBody").alive, bodies);

    const std::uint64_t particles = counted("Particle").alive;
    const std::uint64_t emitters = counted("ParticleEmitter").alive;
    {
        particles2d::Emitter emitter({.texture = device.getWhiteTexture(), .rate = 0.0F, .maxParticles = 32});
        emitter.burst(12);
        EXPECT_EQ(counted("Particle").alive, particles + 12);
        EXPECT_EQ(counted("ParticleEmitter").alive, emitters + 1);
        emitter.clear();
        EXPECT_EQ(counted("Particle").alive, particles);
        emitter.burst(3);
    }
    EXPECT_EQ(counted("Particle").alive, particles);
    EXPECT_EQ(counted("ParticleEmitter").alive, emitters);

    const std::uint64_t tweens = counted("Tween").alive;
    fixture.runLua("tweened = require('haylen.tween').to({x = 0}, 1, {x = 1})");
    EXPECT_EQ(counted("Tween").alive, tweens + 1);
    fixture.runLua("tweened:kill() tweened = nil collectgarbage() collectgarbage()");
    fixture.frames(1);
    EXPECT_EQ(counted("Tween").alive, tweens);
}

TEST(StatsTest, CapturesFramesCountersAndMemory) {
    test::EngineFixture fixture;
    plugins::DebugPlugin& plugin = fixture.engine().getPlugin<plugins::DebugPlugin>();
    fixture.runLua("require('haylen.tween').to({x = 0}, 5, {x = 1}) require('haylen.timer').after(5, function() end) require('haylen.scene').push({})");
    fixture.frames(6);

    const Stats stats = plugin.captureStats(fixture.engine());
    EXPECT_EQ(stats.counts.scenes, 1U);
    EXPECT_EQ(stats.counts.tweens, 1U);
    EXPECT_EQ(stats.counts.timers, 1U);
    EXPECT_EQ(stats.counts.sockets, 0U);
    EXPECT_EQ(stats.frame.fixedSteps, 1U);
    EXPECT_GT(stats.frame.fps, 0.0);
    EXPECT_LE(stats.frame.minimum, stats.frame.average);
    EXPECT_GE(stats.frame.maximum, stats.frame.onePercentLow);
    EXPECT_GE(stats.frame.onePercentLow, stats.frame.average);
    EXPECT_GT(stats.memory.lua, 0U);
    EXPECT_GT(stats.memory.textures, 0);
    EXPECT_EQ(stats.pools.size(), 6U);
    EXPECT_FALSE(stats.objects.empty());
}

TEST(StatsTest, SamplesMonitorsEveryFrame) {
    test::EngineFixture fixture;
    plugins::DebugPlugin& plugin = fixture.engine().getPlugin<plugins::DebugPlugin>();
    int calls = 0;
    plugin.addMonitor("enemies", [&calls] { return ++calls * 2.0; });
    fixture.frames(3);
    ASSERT_EQ(plugin.getMonitors().size(), 1U);
    EXPECT_EQ(plugin.getMonitors()[0]->getValue(), 6.0);
    EXPECT_EQ(plugin.getMonitors()[0]->getHistory(), (std::vector<float>{2.0F, 4.0F, 6.0F}));

    plugin.addMonitor("enemies", [] { return 1.0; });
    EXPECT_EQ(plugin.getMonitors().size(), 1U);
    EXPECT_TRUE(plugin.removeMonitor("enemies"));
    EXPECT_FALSE(plugin.removeMonitor("enemies"));
    EXPECT_THROW(plugin.addMonitor("", [] { return 1.0; }), std::invalid_argument);
    EXPECT_THROW(plugin.addMonitor("nothing", {}), std::invalid_argument);

    // A monitor may remove itself while it samples.
    // clang-format off
    fixture.runLua(R"(
        debugging = require('haylen.debug')
        count = 0
        debugging.addMonitor('count', function() count = count + 1 return count end)
        debugging.addMonitor('once', function() debugging.removeMonitor('once') return 1 end)
    )");
    // clang-format on
    fixture.frames(2);
    EXPECT_EQ(fixture.lua("local monitors = debugging.monitors() return #monitors .. ' ' .. monitors[1].name .. ' ' .. string.format('%g', monitors[1].value) .. ' ' .. #monitors[1].history"), "1 count 2 2");
    EXPECT_EQ(fixture.lua("return tostring(debugging.removeMonitor('count')) .. ' ' .. #debugging.monitors()"), "true 0");
    EXPECT_EQ(fixture.engine().getError(), nullptr);

    fixture.runLua("debugging.addMonitor('broken', function() return 'many' end)");
    fixture.frames(1);
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_NE(std::string(fixture.engine().getError()->what()).find("A debug monitor function returns a number."), std::string::npos);
}

TEST(StatsTest, ReturnsStatisticsToLua) {
    test::EngineFixture fixture;
    fixture.runLua("debugging = require('haylen.debug') require('haylen.scene').push({}) listening = require('haylen.events').on('never', function() end)");
    fixture.frames(3);
    // clang-format off
    EXPECT_EQ(fixture.lua(R"(
        local stats = debugging.stats()
        return table.concat({
            stats.frame.fixedSteps, stats.counts.scenes, tostring(stats.frame.fps > 0), tostring(stats.memory.lua > 0),
            tostring(stats.pools.images.size > stats.pools.images.used), tostring(stats.objects.Texture.alive >= 1),
            stats.objects['haylen.Connection'] and stats.objects['haylen.Connection'].kind or 'none', tostring(stats.rendering.drawCalls >= 0),
        }, ' ')
    )"), "1 1 true true true true userdata true");
    // clang-format on
}

TEST(StatsTest, PublishesObjectEventsWhenAsked) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        debugging = require('haylen.debug')
        local events = require('haylen.events')
        seen = {}
        events.on('object_created', function(data) if data.type == 'haylen.Signal' then seen[#seen + 1] = 'created ' .. data.count end end)
        events.on('object_destroyed', function(data) if data.type == 'haylen.Signal' then seen[#seen + 1] = 'destroyed ' .. data.count end end)
        local quiet = require('haylen.signal').new()
        debugging.setObjectEvents(true)
        local loud = require('haylen.signal').new()
        loud = nil
        collectgarbage() collectgarbage()
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return tostring(debugging.objectEvents()) .. ' ' .. #seen"), "true 0");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("debugging.setObjectEvents(false) return table.concat(seen, ', ')"), "created 1, destroyed 1");
    EXPECT_FALSE(ObjectCounter::isObserved());
}

TEST(StatsTest, StartsWithTheModeOfAppJson) {
    test::EngineFixture fixture({{"app.json", R"({"name": "Stats", "debug": {"stats": "compact", "objectEvents": true}})"}});
    plugins::DebugPlugin& plugin = fixture.engine().getPlugin<plugins::DebugPlugin>();
    EXPECT_EQ(plugin.getStatsMode(), StatsDisplay::Mode::Compact);
    EXPECT_TRUE(plugin.hasObjectEvents());
    EXPECT_EQ(fixture.engine().getConfig().toJson()["debug"]["stats"], "compact");
    EXPECT_THROW((void)core::AppConfig::fromJson(core::Json::parse(R"({"debug": {"stats": "loud"}})")), std::invalid_argument);
    EXPECT_THROW((void)core::AppConfig::fromJson(core::Json::parse(R"({"debug": {"graphs": true}})")), std::invalid_argument);
    EXPECT_EQ(StatsDisplay::modeName(StatsDisplay::Mode::Full), "full");
    EXPECT_EQ(StatsDisplay::modeFromName("off"), StatsDisplay::Mode::Off);
}

// The compact display draws with the renderer on top of everything, and the full overlay draws the window with every section open.
TEST(StatsTest, DrawsTheCompactDisplayAndTheOverlay) {
    test::EngineFixture fixture;
    plugins::DebugPlugin& plugin = fixture.engine().getPlugin<plugins::DebugPlugin>();
    fixture.frames(2);
    const std::size_t quiet = fixture.engine().getRenderer2D().getStats().instances;

    plugin.setStatsMode(StatsDisplay::Mode::Compact);
    fixture.frames(2);
    EXPECT_GT(fixture.engine().getRenderer2D().getStats().instances, quiet);

    // clang-format off
    fixture.runLua(R"(
        local events = require('haylen.events')
        local owner = {}
        events.on('stale', function() end, {owner = owner})
        owner = nil
        collectgarbage()
        require('haylen.signal').new('named')
        require('haylen.debug').addMonitor('frames', function() return 1 end)
    )");
    // clang-format on
    plugin.setStatsMode(StatsDisplay::Mode::Full);
    fixture.frames(3);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    EXPECT_GT(fixture.engine().getRenderer2D().getStats().vertices, 0U);

    // The compact display draws above the error screen too.
    fixture.engine().reportError("broken on purpose");
    plugin.setStatsMode(StatsDisplay::Mode::Off);
    fixture.frames(2);
    const std::size_t failed = fixture.engine().getRenderer2D().getStats().instances;
    plugin.setStatsMode(StatsDisplay::Mode::Compact);
    fixture.frames(2);
    EXPECT_GT(fixture.engine().getRenderer2D().getStats().instances, failed);
}

} // namespace haylen::debug
