#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/assets/Manager.hpp"
#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/FrameClock.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/core/PropertyTrack.hpp"
#include "haylen/core/PropertyTween.hpp"
#include "haylen/core/TimerScheduler.hpp"
#include "haylen/core/TweenManager.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "haylen/lua/Application.hpp"
#include "haylen/lua/Runtime.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "varn/runtime/Runtime.h"

namespace haylen::bench {

// Times the base of the engine on the headless host: the frame loop, events, owners, timers, tweens, jobs and asset loading, and prints the average time of each run.
class CoreBenchmark final {
  public:
    static int run() {
        platform::HeadlessHost host(std::filesystem::temp_directory_path() / "haylen-core-benchmark");
        std::map<std::string, std::vector<std::uint8_t>> files;
        const auto add = [&files](const std::string& path, std::string_view text) { files.insert_or_assign(path, std::vector<std::uint8_t>(text.begin(), text.end())); };
        add("app.json", R"({"name": "Core Benchmark", "identifier": "com.haylen.corebenchmark"})");
        add("source/main.lua", "");
        for (int index = 0; index < kAssets; ++index) {
            add("content/data/item-" + std::to_string(index) + ".json", R"({"name": "item", "stats": {"attack": 12, "defense": 7}, "tags": ["melee", "rare"]})");
        }
        const auto package = std::make_shared<io::MemoryPackage>("core-benchmark", std::move(files));
        core::AppConfig config = core::AppConfig::fromPackage(*package);
        auto application = std::make_unique<lua::Application>();
        application->configure(config);
        core::Engine engine(host, package, std::move(config), std::move(application));
        engine.start();

        std::printf("%-58s %12s %8s\n", "Measurement", "Average ms", "Runs");
        measureFrames(engine);
        measureEvents(engine);
        measureOwners(engine);
        measureTimers();
        measureTweens();
        measureLuaTweens(engine);
        measureJobs(engine);
        measureAssets(engine);
        const bool failed = engine.getError() != nullptr;
        if (failed) {
            std::fprintf(stderr, "%s\n", engine.getError()->what());
        }
        engine.stop();
        return failed ? 1 : 0;
    }

  private:
    struct Box {
        double x = 0.0;
    };

    static constexpr int kAssets = 1000;
    static constexpr int kTimers = 10000;
    static constexpr int kTweens = 10000;
    static constexpr int kListeners = 100;
    static constexpr double kFrameSeconds = 1.0 / 60.0;

    static void measure(const char* name, int runs, const std::function<void()>& work) {
        work();
        const auto start = std::chrono::steady_clock::now();
        for (int run = 0; run < runs; ++run) {
            work();
        }
        std::printf("%-58s %12.4f %8d\n", name, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / runs, runs);
    }

    static void runLua(core::Engine& engine, const std::string& source) {
        lua::Runtime::runChunk(engine.getLuaState(), source, "=benchmark");
    }

    static void measureFrames(core::Engine& engine) {
        measure("frame of an app at rest", 2000, [&engine] { engine.frame(kFrameSeconds); });
        runLua(engine, R"(
            local scene = require('haylen.scene')
            local counter = {value = 0}
            scene.push({update = function(self, dt) counter.value = counter.value + dt end, render = function() end})
        )");
        engine.frame(kFrameSeconds);
        measure("frame of an app with a Lua scene", 2000, [&engine] { engine.frame(kFrameSeconds); });
        runLua(engine, "require('haylen.scene').clear()");
    }

    static void measureEvents(core::Engine& engine) {
        core::EventBus& bus = engine.getEvents();
        std::uint64_t calls = 0;
        std::vector<core::Connection> connections;
        for (int index = 0; index < kListeners; ++index) {
            connections.push_back(bus.on("benchmark.native", [&calls](core::EventBus::Event&) { ++calls; }));
        }
        measure("emit to 100 C++ listeners, 1000 times", 20, [&bus] {
            for (int emit = 0; emit < 1000; ++emit) {
                (void)bus.emit("benchmark.native");
            }
        });
        for (core::Connection& connection : connections) {
            connection.disconnect();
        }

        runLua(engine, R"(
            local events = require('haylen.events')
            benchmarkCalls = 0
            for index = 1, 100 do
                events.on('benchmark.lua', function(amount) benchmarkCalls = benchmarkCalls + amount end)
            end
        )");
        measure("emit from Lua to 100 Lua listeners, 1000 times", 20, [&engine] { runLua(engine, "local events = require('haylen.events') for emit = 1, 1000 do events.emit('benchmark.lua', 1) end"); });
        measure("emit from C++ to 100 Lua listeners, 1000 times", 20, [&bus] {
            for (int emit = 0; emit < 1000; ++emit) {
                (void)bus.emit("benchmark.lua", 1);
            }
        });
        runLua(engine, R"(
            local signal = require('haylen.signal')
            benchmarkSignal = signal.new()
            for index = 1, 100 do
                benchmarkSignal:connect(function(amount) benchmarkCalls = benchmarkCalls + amount end)
            end
        )");
        measure("signal emit to 100 Lua listeners, 1000 times", 20, [&engine] { runLua(engine, "for emit = 1, 1000 do benchmarkSignal:emit(1) end"); });
    }

    // Owners end what they hold when they are collected, which the frame queue finishes at the end of the frame.
    static void measureOwners(core::Engine& engine) {
        measure("1000 owned listeners, timers and tweens created and ended", 20, [&engine] {
            runLua(engine, R"(
                local events = require('haylen.events')
                local timer = require('haylen.timer')
                local tween = require('haylen.tween')
                for index = 1, 1000 do
                    local owner = {x = 0}
                    events.on('benchmark.owned', function() end, {owner = owner})
                    timer.after(10, function() end, {owner = owner})
                    tween.to(owner, 10, {x = 1}, {owner = owner})
                end
                collectgarbage()
                collectgarbage()
            )");
            engine.frame(kFrameSeconds);
        });
    }

    static void measureTimers() {
        core::FrameClock clock(1.0 / 60.0, 0.25);
        core::TimerScheduler timers;
        std::uint64_t fired = 0;
        for (int index = 0; index < kTimers; ++index) {
            (void)timers.every(10.0F + static_cast<float>(index), [&fired] { ++fired; });
        }
        measure("update 10000 waiting timers", 200, [&clock, &timers] {
            clock.advance(kFrameSeconds);
            timers.update(clock);
        });

        core::TimerScheduler due;
        for (int index = 0; index < kTimers; ++index) {
            (void)due.every(0.0F, [&fired] { ++fired; });
        }
        measure("update 10000 timers that fire every frame", 20, [&clock, &due] {
            clock.advance(kFrameSeconds);
            due.update(clock);
        });
    }

    static void measureTweens() {
        core::FrameClock clock(1.0 / 60.0, 0.25);
        core::TweenManager tweens;
        std::vector<Box> boxes(kTweens);
        for (Box& box : boxes) {
            auto tween = std::make_shared<core::PropertyTween>(1000.0F);
            // clang-format off
            tween->addTrack(std::make_unique<core::PropertyTrack>(&box, std::vector<std::string>{"x"},
                [&box] { return core::TweenValue(box.x); },
                [&box](const core::TweenValue& value) { box.x = value.getNumber(); },
                core::TweenProperty::to(100.0)));
            // clang-format on
            tweens.add(std::move(tween));
        }
        measure("update 10000 native tweens", 200, [&clock, &tweens] {
            clock.advance(kFrameSeconds);
            tweens.update(clock);
        });
    }

    // Tweens of Lua tables write their fields through Lua, and a tween with an owner asks the owner for its process mode on every update.
    static void measureLuaTweens(core::Engine& engine) {
        runLua(engine, R"(
            local tween = require('haylen.tween')
            boxes = {}
            for index = 1, 1000 do
                boxes[index] = {x = 0}
                tween.to(boxes[index], 1000, {x = 1})
            end
        )");
        measure("frame with 1000 tweens of Lua tables", 200, [&engine] { engine.frame(kFrameSeconds); });
        runLua(engine, R"(
            local tween = require('haylen.tween')
            local scene = require('haylen.scene')
            tween.killAll()
            level = {}
            scene.push(level)
            for index = 1, 1000 do
                tween.to(boxes[index], 1000, {x = 1}, {owner = level})
            end
        )");
        engine.frame(kFrameSeconds);
        measure("frame with 1000 tweens of Lua tables owned by a scene", 200, [&engine] { engine.frame(kFrameSeconds); });
        runLua(engine, "require('haylen.tween').killAll() require('haylen.scene').clear()");
        engine.frame(kFrameSeconds);
    }

    static void measureJobs(core::Engine& engine) {
        core::JobSystem& jobs = engine.getJobs();
        measure("1000 jobs from the frame thread to a worker and back", 20, [&engine, &jobs] {
            int completed = 0;
            for (int index = 0; index < 1000; ++index) {
                jobs.run([index] { return index * 2; }, [&completed](core::JobSystem::Result<int>) { ++completed; });
            }
            while (completed < 1000) {
                engine.getScriptRuntime().poll();
            }
        });

        std::vector<float> values(1000000, 1.0F);
        measure("parallelFor over a million values", 100, [&jobs, &values] {
            // clang-format off
            jobs.parallelFor(0, values.size(), 4096, [&values](std::size_t begin, std::size_t end) {
                for (std::size_t index = begin; index < end; ++index) {
                    values[index] = values[index] * 1.0001F + 0.5F;
                }
            });
            // clang-format on
        });
    }

    static void measureAssets(core::Engine& engine) {
        assets::Manager& assets = engine.getAssets();
        measure("1000 JSON assets loaded in the background", 5, [&engine, &assets] {
            int loaded = 0;
            std::vector<std::shared_ptr<void>> held;
            held.reserve(kAssets);
            for (int index = 0; index < kAssets; ++index) {
                // clang-format off
                assets.loadAsync("json", "data/item-" + std::to_string(index) + ".json", [&loaded, &held](std::shared_ptr<void> asset, const std::string&) {
                    held.push_back(std::move(asset));
                    ++loaded;
                });
                // clang-format on
            }
            while (loaded < kAssets) {
                engine.frame(kFrameSeconds);
            }
            held.clear();
            (void)assets.releaseUnused();
        });
        measure("1000 JSON assets loaded on the frame thread", 5, [&assets] {
            std::vector<std::shared_ptr<void>> held;
            held.reserve(kAssets);
            for (int index = 0; index < kAssets; ++index) {
                held.push_back(assets.load("json", "data/item-" + std::to_string(index) + ".json"));
            }
            held.clear();
            (void)assets.releaseUnused();
        });
    }
};

} // namespace haylen::bench

int main() {
    return haylen::bench::CoreBenchmark::run();
}
