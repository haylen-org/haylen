#include <lua.hpp>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/physics/Fluid.hpp"
#include "haylen/2d/physics/Ragdoll.hpp"
#include "haylen/2d/physics/Raycaster.hpp"
#include "haylen/2d/physics/World.hpp"
#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "haylen/lua/Application.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Random.hpp"
#include "platform/headless/HeadlessHost.hpp"

namespace haylen::bench {

// Times the physics of the engine on the CPU: the step of piles, stacks, joints and ragdolls on one thread and on the job system, sleeping and sub-steps, fluids, queries, the bulk sync of transforms, the debug drawing and the events that reach Lua.
class PhysicsBenchmark final {
  public:
    static int run() {
        platform::HeadlessHost host(std::filesystem::temp_directory_path() / "haylen-physics-benchmark");
        std::map<std::string, std::vector<std::uint8_t>> files;
        files.emplace("app.json", bytes(R"({"name": "Physics Benchmark", "identifier": "dev.haylen.physics-benchmark"})"));
        files.emplace("source/main.lua", std::vector<std::uint8_t>{});
        const auto package = std::make_shared<io::MemoryPackage>("physics-benchmark", std::move(files));
        core::AppConfig config = core::AppConfig::fromPackage(*package);
        auto application = std::make_unique<lua::Application>();
        application->configure(config);
        core::Engine engine(host, package, std::move(config), std::move(application));
        engine.start();
        core::JobSystem& jobs = engine.getJobs();

        std::printf("Workers of the job system: %zu\n", jobs.getWorkerCount());
        std::printf("%-62s %12s %8s\n", "Scene", "Average ms", "Steps");
        for (const int threads : {1, 2, 4, 8}) {
            measurePile(jobs, threads);
        }
        measureSleeping(jobs);
        for (const int threads : {1, 4}) {
            measurePyramid(jobs, threads);
        }
        measureSubSteps(jobs);
        measureChain(jobs);
        measureRagdolls(jobs);
        for (const std::size_t particles : {1800U, 4000U, 8000U}) {
            measureFluid(jobs, particles);
        }
        measureQueries();
        measureSync();
        measureDebugDraw(engine, host);
        measureLuaEvents(engine);
        engine.stop();
        return 0;
    }

  private:
    static constexpr float kStep = 1.0F / 60.0F;

    [[nodiscard]] static std::vector<std::uint8_t> bytes(std::string_view text) {
        return {text.begin(), text.end()};
    }

    [[nodiscard]] static std::string threadCount(int threads) {
        return std::to_string(threads) + (threads == 1 ? " thread" : " threads");
    }

    static void report(const std::string& name, double milliseconds, int steps) {
        std::printf("%-62s %12.3f %8d\n", name.c_str(), milliseconds, steps);
    }

    // Runs `warmup` steps, then times `steps` steps and reports the average.
    static void measureSteps(const std::string& name, physics2d::World& world, int warmup, int steps) {
        for (int step = 0; step < warmup; ++step) {
            world.step(kStep);
        }
        double total = 0.0;
        for (int step = 0; step < steps; ++step) {
            const auto start = std::chrono::steady_clock::now();
            world.step(kStep);
            total += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        }
        report(name, total / steps, steps);
    }

    static void addPit(physics2d::World& world, float width, float depth) {
        physics2d::Body ground = world.createBody({.type = physics2d::Body::Type::Static});
        ground.addBox({width, 40.0F}, {.offset = {0.0F, 20.0F}});
        ground.addBox({40.0F, depth}, {.offset = {-width * 0.5F, -depth * 0.5F}});
        ground.addBox({40.0F, depth}, {.offset = {width * 0.5F, -depth * 0.5F}});
    }

    static std::vector<physics2d::Body> addBoxes(physics2d::World& world, int count, float width, float size) {
        std::vector<physics2d::Body> bodies;
        const int columns = static_cast<int>(width / (size * 1.5F));
        for (int index = 0; index < count; ++index) {
            const float x = -width * 0.5F + size + static_cast<float>(index % columns) * size * 1.5F;
            const float y = -40.0F - static_cast<float>(index / columns) * size * 1.5F;
            physics2d::Body body = world.createBody({.position = {x, y}});
            body.addBox({size, size});
            bodies.push_back(body);
        }
        return bodies;
    }

    static void measurePile(core::JobSystem& jobs, int threads) {
        physics2d::World world({.threads = threads}, &jobs);
        addPit(world, 2400.0F, 3000.0F);
        (void)addBoxes(world, 4000, 2300.0F, 20.0F);
        measureSteps("Pile of 4000 boxes falling, " + threadCount(threads), world, 0, 240);
    }

    // A settled pile costs almost nothing while it sleeps, and the whole solve without sleeping.
    static void measureSleeping(core::JobSystem& jobs) {
        for (const bool sleeping : {true, false}) {
            physics2d::World world({.threads = 4, .sleepEnabled = sleeping}, &jobs);
            addPit(world, 2400.0F, 3000.0F);
            (void)addBoxes(world, 4000, 2300.0F, 20.0F);
            measureSteps(std::string("Pile of 4000 boxes at rest, sleeping ") + (sleeping ? "on" : "off"), world, 600, 120);
        }
    }

    static void buildPyramid(physics2d::World& world, int rows) {
        addPit(world, 4000.0F, 100.0F);
        constexpr float kSize = 32.0F;
        for (int row = 0; row < rows; ++row) {
            for (int column = 0; column < rows - row; ++column) {
                const float x = (static_cast<float>(column) - static_cast<float>(rows - row) * 0.5F) * (kSize + 0.5F);
                physics2d::Body body = world.createBody({.position = {x, -kSize * 0.5F - static_cast<float>(row) * kSize}, .sleepEnabled = false});
                body.addBox({kSize, kSize});
            }
        }
    }

    static void measurePyramid(core::JobSystem& jobs, int threads) {
        physics2d::World world({.threads = threads}, &jobs);
        buildPyramid(world, 60);
        measureSteps("Pyramid of 60 rows (1830 boxes), " + threadCount(threads), world, 60, 240);
    }

    static void measureSubSteps(core::JobSystem& jobs) {
        for (const int subSteps : {2, 4, 8}) {
            physics2d::World world({.subSteps = subSteps, .threads = 4}, &jobs);
            buildPyramid(world, 40);
            measureSteps("Pyramid of 40 rows, " + std::to_string(subSteps) + " sub-steps", world, 60, 240);
        }
    }

    static void measureChain(core::JobSystem& jobs) {
        physics2d::World world({.threads = 4}, &jobs);
        physics2d::Body anchor = world.createBody({.type = physics2d::Body::Type::Static});
        physics2d::Body previous = anchor;
        constexpr int kLinks = 300;
        for (int link = 0; link < kLinks; ++link) {
            physics2d::Body body = world.createBody({.position = {static_cast<float>(link) * 16.0F + 8.0F, 0.0F}});
            body.addCapsule({-6.0F, 0.0F}, {6.0F, 0.0F}, 2.0F);
            (void)world.createJoint(physics2d::Joint::Type::Revolute, previous, body, {.anchorA = {static_cast<float>(link) * 16.0F, 0.0F}});
            previous = body;
        }
        physics2d::Body ball = world.createBody({.position = {static_cast<float>(kLinks) * 16.0F + 20.0F, 0.0F}});
        ball.addCircle(20.0F, {.density = 20.0F});
        (void)world.createJoint(physics2d::Joint::Type::Revolute, previous, ball, {.anchorA = {static_cast<float>(kLinks) * 16.0F, 0.0F}});
        measureSteps("Chain of 300 links with a heavy ball", world, 0, 240);
    }

    static void measureRagdolls(core::JobSystem& jobs) {
        physics2d::World world({.threads = 4}, &jobs);
        physics2d::Body stairs = world.createBody({.type = physics2d::Body::Type::Static});
        for (int step = 0; step < 10; ++step) {
            const float top = static_cast<float>(step) * 60.0F;
            stairs.addBox({120.0F, 1000.0F - top}, {.offset = {static_cast<float>(step) * 120.0F + 60.0F, top + (1000.0F - top) * 0.5F}});
        }
        std::vector<physics2d::Ragdoll> dolls;
        for (int index = 0; index < 20; ++index) {
            dolls.push_back(physics2d::Ragdoll::create(world, {.position = {60.0F + static_cast<float>(index % 5) * 30.0F, -200.0F - static_cast<float>(index / 5) * 260.0F}, .height = 200.0F, .filter = {.group = -1 - index}, .velocity = {200.0F, 0.0F}}));
        }
        measureSteps("20 ragdolls tumbling down stairs", world, 0, 300);
    }

    // The fluid steps inside the world, so the time of the fluid pass comes from the fluid itself.
    static void measureFluid(core::JobSystem& jobs, std::size_t particles) {
        physics2d::World world({.threads = 4}, &jobs);
        physics2d::Body tank = world.createBody({.type = physics2d::Body::Type::Static});
        tank.addBox({40.0F, 900.0F}, {.offset = {-660.0F, -50.0F}});
        tank.addBox({1360.0F, 40.0F}, {.offset = {0.0F, 400.0F}});
        tank.addBox({40.0F, 900.0F}, {.offset = {660.0F, -50.0F}});
        for (int index = 0; index < 6; ++index) {
            physics2d::Body crate = world.createBody({.position = {-300.0F + static_cast<float>(index) * 120.0F, -400.0F}});
            crate.addBox({70.0F, 50.0F}, {.density = 0.5F});
        }
        physics2d::Fluid fluid(world, {.radius = 6.0F, .smoothingRadius = 22.0F, .maxParticles = particles});
        (void)fluid.fill({-620.0F, -480.0F, 1240.0F, 860.0F});

        double total = 0.0;
        double fluidTotal = 0.0;
        constexpr int kSteps = 240;
        for (int step = 0; step < kSteps; ++step) {
            const auto start = std::chrono::steady_clock::now();
            world.step(kStep);
            total += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            fluidTotal += fluid.getStepMilliseconds();
        }
        const std::string name = "Fluid of " + std::to_string(fluid.size()) + " particles";
        report(name + ", fluid pass", fluidTotal / kSteps, kSteps);
        report(name + ", whole step", total / kSteps, kSteps);
    }

    static void measureQueries() {
        physics2d::World world;
        addPit(world, 2400.0F, 3000.0F);
        (void)addBoxes(world, 2000, 2300.0F, 24.0F);
        for (int step = 0; step < 300; ++step) {
            world.step(kStep);
        }

        math::Random random(7);
        std::vector<math::Vec2> points;
        for (int index = 0; index < 10000; ++index) {
            points.emplace_back(random.range(-1150.0F, 1150.0F), random.range(-800.0F, 0.0F));
        }
        const physics2d::Raycaster raycaster(world);
        const auto start = std::chrono::steady_clock::now();
        int hits = 0;
        for (const math::Vec2 point : points) {
            hits += raycaster.castRay(point, point + math::Vec2{0.0F, 600.0F}) ? 1 : 0;
        }
        const auto middle = std::chrono::steady_clock::now();
        std::size_t found = 0;
        for (const math::Vec2 point : points) {
            found += world.queryCircle(point, 40.0F).size();
        }
        const auto end = std::chrono::steady_clock::now();
        report("10000 ray casts through a pile of 2000 boxes", std::chrono::duration<double, std::milli>(middle - start).count(), 1);
        report("10000 circle queries in a pile of 2000 boxes", std::chrono::duration<double, std::milli>(end - middle).count(), 1);
        std::printf("%-62s %12d %8zu\n", "Hits and shapes found", hits, found);
    }

    static void measureSync() {
        for (const bool blended : {false, true}) {
            physics2d::World world({.interpolate = blended});
            addPit(world, 2400.0F, 3000.0F);
            const std::vector<physics2d::Body> bodies = addBoxes(world, 5000, 2300.0F, 16.0F);
            world.step(kStep);
            std::vector<float> values(bodies.size() * 3);
            constexpr int kRuns = 200;
            const auto start = std::chrono::steady_clock::now();
            for (int run = 0; run < kRuns; ++run) {
                world.readTransforms(bodies, values, blended ? std::optional(0.5F) : std::nullopt);
            }
            report(std::string("Read the transforms of 5000 bodies") + (blended ? ", interpolated" : ""), std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / kRuns, kRuns);
        }
    }

    static void measureDebugDraw(core::Engine& engine, platform::HeadlessHost& host) {
        physics2d::World world;
        addPit(world, 2400.0F, 3000.0F);
        (void)addBoxes(world, 2000, 2300.0F, 24.0F);
        graphics2d::Renderer& renderer = engine.getRenderer2D();
        const graphics2d::Camera camera;
        constexpr int kRuns = 60;
        double total = 0.0;
        for (int run = 0; run < kRuns; ++run) {
            renderer.beginFrame(engine.getViewport(), math::Color::black());
            renderer.beginWorld(camera);
            const auto start = std::chrono::steady_clock::now();
            world.debugDraw(renderer);
            total += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            renderer.endFrame(host.getFrameTarget());
        }
        report("Debug drawing of 2000 boxes", total / kRuns, kRuns);
    }

    static bool runLua(lua_State* L, const char* source) {
        if (luaL_dostring(L, source) == LUA_OK) {
            return true;
        }
        std::fprintf(stderr, "%s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }

    // Steps a pile from Lua with and without the hit and contact callbacks set, which is what reaches scripts.
    static void measureLuaEvents(core::Engine& engine) {
        lua_State* L = engine.getLuaState();
        // clang-format off
        const char* setup = R"(
            local physics2d = require('haylen.physics2d')
            function benchmarkPile()
                local world = physics2d.newWorld({threads = 1})
                local ground = world:createBody({type = 'static'})
                ground:addBox(2400, 40, {offsetY = 20})
                ground:addBox(40, 3000, {offsetX = -1200, offsetY = -1500})
                ground:addBox(40, 3000, {offsetX = 1200, offsetY = -1500})
                for index = 0, 1999 do
                    local box = world:createBody({x = -1140 + (index % 63) * 36, y = -40 - (index // 63) * 36})
                    box:addBox(24, 24)
                end
                return world
            end
            benchmarkEvents = 0
            quietWorld = benchmarkPile()
            busyWorld = benchmarkPile()
            busyWorld.onHit = function(a, b, contact) benchmarkEvents = benchmarkEvents + 1 end
            busyWorld.onContactBegin = function(a, b, contact) benchmarkEvents = benchmarkEvents + 1 end
        )";
        // clang-format on
        if (!runLua(L, setup)) {
            return;
        }
        for (const std::string world : {"quietWorld", "busyWorld"}) {
            const std::string steps = "for step = 1, 240 do " + world + ":step(1 / 60) end";
            const auto start = std::chrono::steady_clock::now();
            if (!runLua(L, steps.c_str())) {
                return;
            }
            const double milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / 240.0;
            report(world == "busyWorld" ? "Pile of 2000 boxes stepped from Lua with callbacks" : "Pile of 2000 boxes stepped from Lua", milliseconds, 240);
        }
        lua_getglobal(L, "benchmarkEvents");
        std::printf("%-62s %12.0f %8d\n", "Events that reached Lua", lua_tonumber(L, -1), 240);
        lua_pop(L, 1);
        (void)runLua(L, "quietWorld, busyWorld = nil, nil collectgarbage()");
    }
};

} // namespace haylen::bench

int main() {
    return haylen::bench::PhysicsBenchmark::run();
}
