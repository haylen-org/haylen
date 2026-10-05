#include <lua.hpp>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/particles/Emitter.hpp"
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

// Times particle emitters on the CPU: the simulation of one large emitter on one thread and on the job system, many small emitters, spawning, the instances that drawing builds, and emitters driven from Lua.
class ParticleBenchmark final {
  public:
    static int run() {
        platform::HeadlessHost host(std::filesystem::temp_directory_path() / "haylen-particle-benchmark");
        std::map<std::string, std::vector<std::uint8_t>> files;
        files.emplace("app.json", bytes(R"({"name": "Particle Benchmark", "identifier": "dev.haylen.particle-benchmark"})"));
        files.emplace("source/main.lua", std::vector<std::uint8_t>{});
        const auto package = std::make_shared<io::MemoryPackage>("particle-benchmark", std::move(files));
        core::AppConfig config = core::AppConfig::fromPackage(*package);
        auto application = std::make_unique<lua::Application>();
        application->configure(config);
        core::Engine engine(host, package, std::move(config), std::move(application));
        engine.start();
        const graphics::Texture texture = engine.getGraphics().createTexture(graphics::Image(16, 16, math::Color::white()));

        std::printf("Workers of the job system: %zu\n", engine.getJobs().getWorkerCount());
        std::printf("%-62s %12s %8s\n", "Scene", "Average ms", "Frames");
        measureLarge(engine, texture, false);
        measureLarge(engine, texture, true);
        measureMany(engine, texture);
        measureSpawning(engine, texture);
        measureDraw(engine, host, texture);
        measureLua(engine);
        engine.stop();
        return 0;
    }

  private:
    static constexpr float kStep = 1.0F / 60.0F;
    static constexpr int kFrames = 240;

    [[nodiscard]] static std::vector<std::uint8_t> bytes(std::string_view text) {
        return {text.begin(), text.end()};
    }

    static void report(const std::string& name, double milliseconds, int frames) {
        std::printf("%-62s %12.3f %8d\n", name.c_str(), milliseconds, frames);
    }

    // A fountain whose rate keeps about `count` particles alive, with gravity, damping and both accelerations, as busy effects use.
    [[nodiscard]] static particles2d::EmitterConfig fountain(const graphics::Texture& texture, std::size_t count) {
        return {
            .texture = texture,
            .rate = static_cast<float>(count) / 1.5F,
            .prewarm = 2.0F,
            .maxParticles = count + count / 4,
            .lifetime = {1.0F, 2.0F},
            .speed = {100.0F, 300.0F},
            .spread = 1.2F,
            .gravity = {0.0F, 400.0F},
            .radialAcceleration = {-20.0F, 20.0F},
            .tangentialAcceleration = {0.0F, 30.0F},
            .damping = 0.4F,
            .startSize = {8.0F, 12.0F},
            .endSize = {0.0F, 2.0F},
            .spin = {-2.0F, 2.0F},
            .colors = {math::Color::white(), math::Color{1.0F, 0.6F, 0.2F, 1.0F}, math::Color::transparent()},
            .shape = particles2d::EmitterConfig::Shape::Circle,
            .shapeSize = {20.0F, 0.0F},
        };
    }

    // Times `frames` updates of the emitters after one update that runs their prewarm.
    static double timeUpdates(std::vector<particles2d::Emitter>& emitters, core::JobSystem* jobs) {
        for (particles2d::Emitter& emitter : emitters) {
            emitter.update(kStep);
        }
        const auto start = std::chrono::steady_clock::now();
        for (int frame = 0; frame < kFrames; ++frame) {
            for (particles2d::Emitter& emitter : emitters) {
                if (jobs != nullptr) {
                    emitter.update(kStep, *jobs);
                } else {
                    emitter.update(kStep);
                }
            }
        }
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / kFrames;
    }

    static void measureLarge(core::Engine& engine, const graphics::Texture& texture, bool parallel) {
        std::vector<particles2d::Emitter> emitters;
        emitters.emplace_back(fountain(texture, 100000), 1);
        const double milliseconds = timeUpdates(emitters, parallel ? &engine.getJobs() : nullptr);
        report(std::string("One emitter of 100000 particles, ") + (parallel ? "job system" : "one thread"), milliseconds, kFrames);
    }

    static void measureMany(core::Engine& engine, const graphics::Texture& texture) {
        std::vector<particles2d::Emitter> emitters;
        for (std::uint64_t index = 0; index < 1000; ++index) {
            emitters.emplace_back(fountain(texture, 100), index);
        }
        report("1000 emitters of 100 particles", timeUpdates(emitters, &engine.getJobs()), kFrames);
    }

    // Short lives replace every particle several times per second, so the cost is mostly spawning.
    static void measureSpawning(core::Engine& engine, const graphics::Texture& texture) {
        particles2d::EmitterConfig config = fountain(texture, 20000);
        config.lifetime = {0.1F, 0.1F};
        config.rate = 200000.0F;
        config.prewarm = 0.0F;
        std::vector<particles2d::Emitter> emitters;
        emitters.emplace_back(config, 2);
        report("Spawning 200000 particles per second", timeUpdates(emitters, &engine.getJobs()), kFrames);
    }

    // Times the draw calls of the emitters alone, which build the instances of every particle, inside real frames.
    static void measureDraw(core::Engine& engine, platform::HeadlessHost& host, const graphics::Texture& texture) {
        graphics2d::Renderer& renderer = engine.getRenderer2D();
        const graphics2d::Camera camera;
        for (const auto& [count, size] : {std::pair<std::size_t, std::size_t>{1, 100000}, std::pair<std::size_t, std::size_t>{1000, 100}}) {
            std::vector<particles2d::Emitter> emitters;
            for (std::size_t index = 0; index < count; ++index) {
                emitters.emplace_back(fountain(texture, size), index);
                emitters.back().update(kStep);
            }
            double total = 0.0;
            for (int frame = 0; frame < kFrames; ++frame) {
                renderer.beginFrame(engine.getViewport(), math::Color::black());
                renderer.beginWorld(camera);
                const auto start = std::chrono::steady_clock::now();
                for (const particles2d::Emitter& emitter : emitters) {
                    emitter.draw(renderer);
                }
                total += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
                renderer.endFrame(host.getFrameTarget());
            }
            report("Drawing " + std::to_string(count) + (count == 1 ? " emitter of " : " emitters of ") + std::to_string(size) + " particles", total / kFrames, kFrames);
        }
    }

    static bool runLua(lua_State* L, const char* source) {
        if (luaL_dostring(L, source) == LUA_OK) {
            return true;
        }
        std::fprintf(stderr, "%s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }

    // Updates many emitters from Lua every frame, one call each, which is what scenes full of effects cost the frame thread.
    static void measureLua(core::Engine& engine) {
        lua_State* L = engine.getLuaState();
        // clang-format off
        const char* setup = R"(
            local graphics = require('haylen.graphics')
            local particles2d = require('haylen.particles2d')
            benchmarkEmitters = {}
            for index = 1, 500 do
                benchmarkEmitters[index] = particles2d.newEmitter({
                    texture = graphics.whiteTexture(), rate = 40, prewarm = 2, lifetime = {1, 2}, speed = {50, 150}, spread = 1,
                    gravity = {0, 200}, startSize = 8, endSize = 0, colors = {'#FFFFFFFF', '#00FF8000'}, seed = index,
                })
                benchmarkEmitters[index].position = {index * 3, 0}
            end
            function benchmarkFrame()
                for index = 1, #benchmarkEmitters do
                    benchmarkEmitters[index]:update(1 / 60)
                end
            end
            benchmarkFrame()
        )";
        // clang-format on
        if (!runLua(L, setup)) {
            return;
        }
        const auto start = std::chrono::steady_clock::now();
        if (!runLua(L, "for frame = 1, 240 do benchmarkFrame() end")) {
            return;
        }
        report("500 emitters of about 60 particles updated from Lua", std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / kFrames, kFrames);
        (void)runLua(L, "benchmarkEmitters = nil collectgarbage()");
    }
};

} // namespace haylen::bench

int main() {
    return haylen::bench::ParticleBenchmark::run();
}
