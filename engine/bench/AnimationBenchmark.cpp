#include <lua.hpp>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "haylen/2d/animation/AnimatedBatch.hpp"
#include "haylen/2d/animation/Animation.hpp"
#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "haylen/lua/Application.hpp"
#include "haylen/math/Color.hpp"
#include "platform/headless/HeadlessHost.hpp"

namespace haylen::bench {

// Times animation on the CPU: an animated batch of many sprites on one thread and on the job system, and a crowd animated from Lua with one animator per unit and with one animated batch.
class AnimationBenchmark final {
  public:
    static int run() {
        platform::HeadlessHost host(std::filesystem::temp_directory_path() / "haylen-animation-benchmark");
        std::map<std::string, std::vector<std::uint8_t>> files;
        files.emplace("app.json", bytes(R"({"name": "Animation Benchmark", "identifier": "dev.haylen.animation-benchmark"})"));
        files.emplace("source/main.lua", std::vector<std::uint8_t>{});
        const auto package = std::make_shared<io::MemoryPackage>("animation-benchmark", std::move(files));
        core::AppConfig config = core::AppConfig::fromPackage(*package);
        auto application = std::make_unique<lua::Application>();
        application->configure(config);
        core::Engine engine(host, package, std::move(config), std::move(application));
        engine.start();
        const graphics::Texture texture = engine.getGraphics().createTexture(graphics::Image(256, 32, math::Color::white()));

        std::printf("Workers of the job system: %zu\n", engine.getJobs().getWorkerCount());
        std::printf("%-62s %12s %8s\n", "Scene", "Average ms", "Frames");
        measureBatch(engine, texture, false);
        measureBatch(engine, texture, true);
        measureLua(engine, "2000 units with one animator each, updated and applied from Lua", R"(
            for index = 1, #benchmarkUnits do
                local unit = benchmarkUnits[index]
                unit.animator:update(1 / 60)
                unit.animator:apply(unit.sprite)
            end
        )");
        measureLua(engine, "2000 units of one animated batch, updated from Lua", "benchmarkCrowd:update(1 / 60)");
        engine.stop();
        return 0;
    }

  private:
    static constexpr int kFrames = 240;

    [[nodiscard]] static std::vector<std::uint8_t> bytes(std::string_view text) {
        return {text.begin(), text.end()};
    }

    static void report(const std::string& name, double milliseconds) {
        std::printf("%-62s %12.3f %8d\n", name.c_str(), milliseconds, kFrames);
    }

    static bool runLua(lua_State* L, const char* source) {
        if (luaL_dostring(L, source) == LUA_OK) {
            return true;
        }
        std::fprintf(stderr, "%s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }

    static void measureBatch(core::Engine& engine, const graphics::Texture& texture, bool parallel) {
        animation2d::AnimatedBatch batch(texture);
        batch.addAnimation("walk", animation2d::Animation::fromGrid(texture, {.frameSize = {32.0F, 32.0F}, .framesPerSecond = 12.0F}));
        batch.reserve(100000);
        for (int index = 0; index < 100000; ++index) {
            (void)batch.add({.position = {static_cast<float>(index % 400) * 4.0F, static_cast<float>(index / 400) * 4.0F}}, "walk", 0.8F + static_cast<float>(index % 5) * 0.1F);
        }
        const auto start = std::chrono::steady_clock::now();
        for (int frame = 0; frame < kFrames; ++frame) {
            batch.update(1.0F / 60.0F, parallel ? &engine.getJobs() : nullptr);
        }
        report(std::string("Animated batch of 100000 sprites, ") + (parallel ? "job system" : "one thread"), std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / kFrames);
    }

    // Builds 2000 units both ways on a sheet of 8 frames and times the body of a frame.
    static void measureLua(core::Engine& engine, const std::string& name, const std::string& body) {
        lua_State* L = engine.getLuaState();
        // clang-format off
        const std::string setup = R"(
            local animation2d = require('haylen.animation2d')
            local graphics = require('haylen.graphics')
            local graphics2d = require('haylen.graphics2d')
            local sheet = graphics.newTexture(256, 32)
            local walk = animation2d.fromGrid(sheet, {frameWidth = 32, frameHeight = 32, framesPerSecond = 12})
            benchmarkUnits = {}
            benchmarkCrowd = animation2d.newAnimatedBatch(sheet, {walk = walk})
            for index = 1, 2000 do
                local animator = animation2d.newAnimator()
                animator:add('walk', walk)
                animator:play('walk')
                benchmarkUnits[index] = {animator = animator, sprite = graphics2d.newSprite(sheet, {x = index, y = 0})}
                benchmarkCrowd:add({x = index, y = 0, speed = 0.8 + (index % 5) * 0.1})
            end
            function benchmarkFrame()
        )" + body + R"(
            end
        )";
        // clang-format on
        if (!runLua(L, setup.c_str())) {
            return;
        }
        const auto start = std::chrono::steady_clock::now();
        if (!runLua(L, "for frame = 1, 240 do benchmarkFrame() end")) {
            return;
        }
        report(name, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / kFrames);
        (void)runLua(L, "benchmarkUnits = nil benchmarkCrowd = nil collectgarbage()");
    }
};

} // namespace haylen::bench

int main() {
    return haylen::bench::AnimationBenchmark::run();
}
