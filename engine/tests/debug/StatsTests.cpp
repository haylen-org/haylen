#include <gtest/gtest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "2d/graphics/GpuInstance.hpp"
#include "graphics/Gpu.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/graphics/SpriteInstance.hpp"
#include "haylen/2d/graphics/StaticSpriteBatch.hpp"
#include "haylen/2d/particles/Emitter.hpp"
#include "haylen/2d/physics/World.hpp"
#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Connection.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/debug/ObjectCounter.hpp"
#include "haylen/debug/Stats.hpp"
#include "haylen/debug/TrackedCount.hpp"
#include "haylen/debug/TrackedObject.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/plugins/DebugPlugin.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "haylen/ui/Backend.hpp"
#include "support/AllocationTracker.hpp"
#include "support/EngineFixture.hpp"
#include "support/RecordingScene.hpp"

namespace haylen::debug {

class ObjectCounterTest : public ::testing::Test {
  protected:
    // Reads a counter the way the statistics do, as zero before its first object.
    [[nodiscard]] static ObjectCounter::Snapshot counted(std::string_view name) {
        return ObjectCounter::find(name).value_or(ObjectCounter::Snapshot{});
    }

    [[nodiscard]] static graphics::Device::Pool pool(graphics::Device& device, std::string_view name) {
        for (const graphics::Device::Pool& found : device.getPools()) {
            if (found.name == name) {
                return found;
            }
        }
        return {};
    }
};

TEST_F(ObjectCounterTest, CountsTrackedObjectsAndTheirMemory) {
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

TEST_F(ObjectCounterTest, CountsLuaUserdataUntilTheyAreCollected) {
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

TEST_F(ObjectCounterTest, CountsEngineResources) {
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

// Static batches report the bytes of their instance buffers, and the renderer the bytes of the buffers it streams batches through.
TEST(StatsTest, ReportsTheGpuMemoryOfBatches) {
    test::EngineFixture fixture;
    plugins::DebugPlugin& plugin = fixture.engine().getPlugin<plugins::DebugPlugin>();
    const std::int64_t before = plugin.captureStats(fixture.engine()).memory.batches;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(4, 4));
    std::vector<graphics2d::SpriteInstance> sprites(1000);
    graphics2d::StaticSpriteBatch batch = fixture.engine().getRenderer2D().createStaticBatch(texture, sprites);
    fixture.runLua("local g = require('haylen.graphics2d') require('haylen.scene').push({render = function() g.beginScreen() g.drawRect({0, 0, 10, 10}, '#FFFFFFFF') end})");
    fixture.frames(2);

    const Stats stats = plugin.captureStats(fixture.engine());
    EXPECT_EQ(stats.memory.batches - before, static_cast<std::int64_t>(1000 * sizeof(graphics2d::GpuInstance)));
    EXPECT_GT(stats.memory.buffers, 0);
    EXPECT_EQ(fixture.lua("local memory = require('haylen.debug').stats().memory return tostring(memory.batches > 0 and memory.buffers > 0)"), "true");

    batch = {};
    fixture.frames(2);
    EXPECT_EQ(plugin.captureStats(fixture.engine()).memory.batches, before);
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

TEST(StatsTest, MonitorConnectionsRemoveAndHoldMonitors) {
    test::EngineFixture fixture;
    plugins::DebugPlugin& plugin = fixture.engine().getPlugin<plugins::DebugPlugin>();
    int calls = 0;
    core::Connection connection = plugin.addMonitor("enemies", [&calls] { return ++calls; });
    fixture.frames(1);
    connection.setBlocked(true);
    fixture.frames(2);
    EXPECT_TRUE(connection.isBlocked());
    connection.setBlocked(false);
    fixture.frames(1);
    EXPECT_EQ(plugin.getMonitors()[0]->getHistory(), (std::vector<float>{1.0F, 2.0F}));

    // A replaced monitor ends its connection, so disconnecting it leaves the replacement alone.
    core::Connection replacement = plugin.addMonitor("enemies", [] { return 5.0; });
    EXPECT_FALSE(connection.isConnected());
    connection.disconnect();
    ASSERT_EQ(plugin.getMonitors().size(), 1U);
    replacement.disconnect();
    EXPECT_TRUE(plugin.getMonitors().empty());
    EXPECT_FALSE(replacement.isConnected());

    // A sampler that returns nothing skips the frame and keeps the last value.
    plugin.addMonitor("even", [&calls]() -> std::optional<double> { return ++calls % 2 == 0 ? std::optional<double>(calls) : std::nullopt; });
    fixture.frames(4);
    EXPECT_EQ(plugin.getMonitors()[0]->getHistory().size(), 2U);
    EXPECT_EQ(plugin.getMonitors()[0]->getValue(), 6.0);
}

TEST(StatsTest, EndsMonitorsWithTheirOwner) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        debugging = require('haylen.debug')
        scene = require('haylen.scene')
        level = {}
        local held = {}
        owned = debugging.addMonitor('level', function() return 1 end, {owner = level})
        debugging.addMonitor('held', function() return 2 end, {owner = held})
        free = debugging.addMonitor('free', function() return 3 end)
        blocked = debugging.addMonitor('blocked', function() blockedCalls = (blockedCalls or 0) + 1 return 4 end)
        blocked.blocked = true
        scene.push(level)
        held = nil
        collectgarbage()
        collectgarbage()
    )");
    // clang-format on
    fixture.frames(2);
    EXPECT_EQ(fixture.lua("local names = {} for _, monitor in ipairs(debugging.monitors()) do names[#names + 1] = monitor.name end return table.concat(names, ' ')"), "level free blocked");

    fixture.runLua("scene.pop()");
    fixture.frames(2);
    EXPECT_EQ(fixture.lua("local names = {} for _, monitor in ipairs(debugging.monitors()) do names[#names + 1] = monitor.name end return table.concat(names, ' ') .. ' ' .. tostring(owned.connected) .. ' ' .. tostring(blockedCalls)"), "free blocked false nil");
    EXPECT_EQ(fixture.lua("free:disconnect() return #debugging.monitors() .. ' ' .. tostring(free.connected)"), "1 false");
    EXPECT_NE(fixture.lua("debugging.addMonitor('weak', function() return 1 end, {weak = true})").find("Unknown option \"weak\""), std::string::npos);
    EXPECT_NE(fixture.lua("debugging.addMonitor('number', function() return 1 end, {owner = 5})").find("An owner must be a table or a userdata, not number."), std::string::npos);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
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
        events.on('objectCreated', function(data) if data.type == 'haylen.Signal' then seen[#seen + 1] = 'created ' .. data.count end end)
        events.on('objectDestroyed', function(data) if data.type == 'haylen.Signal' then seen[#seen + 1] = 'destroyed ' .. data.count end end)
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
    EXPECT_EQ(plugin.getStatsDisplay().getItems(), StatsDisplay::getDefaultItems());
}

// The items of the compact display come from the list of the app, which also writes back.
TEST(StatsTest, StartsWithTheItemsOfAppJson) {
    test::EngineFixture picked({{"app.json", R"({"name": "Stats", "debug": {"statsItems": ["physics", "frameRate"]}})"}});
    StatsDisplay& display = picked.engine().getPlugin<plugins::DebugPlugin>().getStatsDisplay();
    EXPECT_EQ(display.getItems(), (std::vector{StatsDisplay::Item::FrameRate, StatsDisplay::Item::Physics}));
    EXPECT_EQ(picked.engine().getConfig().toJson()["debug"]["statsItems"], core::Json::parse(R"(["physics", "frameRate"])"));
    EXPECT_THROW((void)core::AppConfig::fromJson(core::Json::parse(R"({"debug": {"statsItems": ["fps"]}})")), std::invalid_argument);
    EXPECT_EQ(StatsDisplay::itemFromName("particles"), StatsDisplay::Item::Particles);
    EXPECT_EQ(StatsDisplay::itemName(StatsDisplay::Item::DrawCalls), "drawCalls");
}

// The compact display draws a line for each item it shows, so the player picks how much it covers, and nothing at all without items.
TEST(StatsTest, DrawsALineForEveryItemItShows) {
    test::EngineFixture fixture;
    plugins::DebugPlugin& plugin = fixture.engine().getPlugin<plugins::DebugPlugin>();
    StatsDisplay& display = plugin.getStatsDisplay();
    fixture.frames(2);
    const std::size_t quiet = fixture.engine().getRenderer2D().getStats().instances;
    plugin.setStatsMode(StatsDisplay::Mode::Compact);
    fixture.frames(2);
    const std::size_t standard = fixture.engine().getRenderer2D().getStats().instances;

    for (const auto& [name, item] : StatsDisplay::kItemNames) {
        display.setShowing(item, true);
    }
    fixture.frames(2);
    EXPECT_GT(fixture.engine().getRenderer2D().getStats().instances, standard);

    display.setItems({});
    fixture.frames(2);
    EXPECT_EQ(fixture.engine().getRenderer2D().getStats().instances, quiet);
    EXPECT_EQ(fixture.engine().getError(), nullptr);

    fixture.runLua("debugging = require('haylen.debug')");
    EXPECT_EQ(fixture.lua("debugging.setStatsItem('physics', true) debugging.setStatsItem('audio', true) return table.concat(debugging.statsItems(), ' ') .. ' ' .. tostring(debugging.statsItem('memory'))"), "physics audio false");
    EXPECT_NE(fixture.lua("debugging.setStatsItem('fps', true)").find("unknown value 'fps'"), std::string::npos);
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

// The compact display keeps its lines while the numbers stay, and the monitors and drawings walk copies they keep, so those frames allocate no more than frames without them.
TEST(StatsTest, DrawsTheCompactDisplayWithoutAllocating) {
    test::EngineFixture fixture;
    plugins::DebugPlugin& plugin = fixture.engine().getPlugin<plugins::DebugPlugin>();
    // clang-format off
    const auto measure = [&fixture] {
        fixture.frames(5, 1.0 / 240.0);
        const test::AllocationTracker tracker;
        fixture.frames(10, 1.0 / 240.0);
        return tracker.getTotal();
    };
    // clang-format on
    const std::size_t quiet = measure();

    const core::Connection monitor = plugin.addMonitor("frames", [] { return std::optional(1.0); });
    const core::Connection drawer = plugin.addDrawer("marks", [](graphics2d::Renderer& renderer) { renderer.drawRect({0.0F, 0.0F, 4.0F, 4.0F}, math::Color::white()); });
    plugin.setDrawing("marks", true);
    plugin.setStatsMode(StatsDisplay::Mode::Compact);
    for (const auto& [name, item] : StatsDisplay::kItemNames) {
        plugin.getStatsDisplay().setShowing(item, true);
    }
    EXPECT_EQ(measure(), quiet);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

// The overlay lists the scenes, the log at every level, the content, the objects with their memory and the events with their listeners once their sections open.
TEST(StatsTest, DrawsEverySectionOfTheOverlay) {
    std::vector<std::string> entries;
    test::EngineFixture fixture;
    plugins::DebugPlugin& plugin = fixture.engine().getPlugin<plugins::DebugPlugin>();
    fixture.engine().getScenes().push(std::make_shared<test::RecordingScene>("first", entries));
    fixture.runLua("require('haylen.events').on('tick', function() end) require('haylen.signal').new('named')");
    for (const core::Log::Level level : {core::Log::Level::Debug, core::Log::Level::Info, core::Log::Level::Warning, core::Log::Level::Error}) {
        core::Log::write(level, "A line of the overlay log.");
    }
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(1024, 1024, math::Color::white()));
    plugin.setStatsMode(StatsDisplay::Mode::Full);
    fixture.frames(2);

    // The closed sections open as a click on their headers would open them.
    fixture.engine().getPlugin<plugins::UiPlugin>().getBackend().makeCurrent();
    ImGuiWindow* window = ImGui::FindWindowByName("Haylen debug");
    ASSERT_NE(window, nullptr);
    window->StateStorage.SetInt(window->GetID("Content"), 1);
    window->StateStorage.SetInt(window->GetID("Objects"), 1);
    window->StateStorage.SetInt(window->GetID("Signals and events"), 1);
    fixture.frames(2);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    EXPECT_EQ(plugin.getStatsMode(), StatsDisplay::Mode::Full);
}

} // namespace haylen::debug
