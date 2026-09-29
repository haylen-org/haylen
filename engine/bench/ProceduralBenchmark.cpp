#include <chrono>
#include <cstdio>
#include <functional>
#include <vector>

#include "haylen/2d/physics/Terrain.hpp"
#include "haylen/2d/physics/World.hpp"
#include "haylen/2d/procedural/Delaunay.hpp"
#include "haylen/2d/procedural/Region.hpp"
#include "haylen/2d/procedural/Scatter.hpp"
#include "haylen/2d/procedural/WaveFunctionCollapse.hpp"
#include "haylen/math/MarchingSquares.hpp"
#include "haylen/math/Noise2D.hpp"
#include "haylen/math/PoissonDisk.hpp"
#include "haylen/math/Random.hpp"

namespace haylen::bench {

// Times the heavy procedural algorithms on the CPU and prints the average time of each.
class ProceduralBenchmark final {
  public:
    static int run() {
        std::printf("%-46s %12s %8s\n", "Algorithm", "Average ms", "Runs");

        const procedural2d::WaveFunctionCollapse::Rules coast = coastRules();
        // clang-format off
        measure("Wave function collapse 64x64, 3 tiles", 10, [&coast] {
            math::Random random(1);
            (void)procedural2d::WaveFunctionCollapse::generate(coast, {.width = 64, .height = 64}, random);
        });
        measure("Poisson disk 2048x2048, distance 16", 10, [] {
            math::Random random(2);
            (void)math::PoissonDisk::sample({.area = {0.0F, 0.0F, 2048.0F, 2048.0F}, .minimumDistance = 16.0F}, random);
        });
        measure("Poisson disk 2048x2048, distance 16 to 64", 10, [] {
            math::Random random(3);
            math::PoissonDisk::Options options{.area = {0.0F, 0.0F, 2048.0F, 2048.0F}, .minimumDistance = 16.0F, .maximumDistance = 64.0F};
            options.distance = [](math::Vec2 point) { return 16.0F + point.x / 2048.0F * 48.0F; };
            (void)math::PoissonDisk::sample(options, random);
        });
        measure("Scatter random 4096x4096, 0.001 per unit", 10, [] {
            math::Random random(4);
            (void)procedural2d::Scatter::generate(procedural2d::Region::rect({0.0F, 0.0F, 4096.0F, 4096.0F}), {.density = 0.001F, .weights = {1.0F, 2.0F, 3.0F}}, random);
        });
        // clang-format on

        const std::vector<float> field = noiseField(1024);
        measure("Marching squares 1024x1024 noise field", 10, [&field] { (void)math::MarchingSquares::trace(field, 1024, 1024, {.threshold = 0.0F}); });

        const std::vector<math::Vec2> points = randomPoints(100000);
        measure("Delaunay 100000 points", 5, [&points] { (void)procedural2d::Delaunay(points); });

        measureTerrain();
        return 0;
    }

  private:
    static void measure(const char* name, int runs, const std::function<void()>& work) {
        work();
        const auto start = std::chrono::steady_clock::now();
        for (int run = 0; run < runs; ++run) {
            work();
        }
        report(name, std::chrono::steady_clock::now() - start, runs);
    }

    static void report(const char* name, std::chrono::steady_clock::duration elapsed, int runs) {
        std::printf("%-46s %12.3f %8d\n", name, std::chrono::duration<double, std::milli>(elapsed).count() / runs, runs);
    }

    [[nodiscard]] static procedural2d::WaveFunctionCollapse::Rules coastRules() {
        procedural2d::WaveFunctionCollapse::Rules rules(3);
        for (const auto direction : {procedural2d::WaveFunctionCollapse::Direction::Right, procedural2d::WaveFunctionCollapse::Direction::Down}) {
            rules.allow(0, 0, direction);
            rules.allow(1, 1, direction);
            rules.allow(2, 2, direction);
            rules.allow(0, 1, direction);
            rules.allow(1, 0, direction);
            rules.allow(1, 2, direction);
            rules.allow(2, 1, direction);
        }
        return rules;
    }

    [[nodiscard]] static std::vector<float> noiseField(int size) {
        const math::Noise2D noise(5);
        std::vector<float> values;
        values.reserve(static_cast<std::size_t>(size) * static_cast<std::size_t>(size));
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                values.push_back(noise.fractal(static_cast<float>(x) / 64.0F, static_cast<float>(y) / 64.0F));
            }
        }
        return values;
    }

    [[nodiscard]] static std::vector<math::Vec2> randomPoints(int count) {
        math::Random random(6);
        std::vector<math::Vec2> points;
        points.reserve(static_cast<std::size_t>(count));
        for (int index = 0; index < count; ++index) {
            points.push_back({random.range(0.0F, 10000.0F), random.range(0.0F, 10000.0F)});
        }
        return points;
    }

    // Builds a 2048 by 1024 terrain once, then carves one crater at a time and rebuilds the chunks it touched, the way an explosion does.
    static void measureTerrain() {
        physics2d::World world;
        physics2d::Terrain terrain(world, {.columns = 513, .rows = 257, .cellSize = 4.0F, .chunkSize = 32});
        const std::vector<math::Vec2> ground{{0.0F, 300.0F}, {2048.0F, 300.0F}, {2048.0F, 1024.0F}, {0.0F, 1024.0F}};

        const auto start = std::chrono::steady_clock::now();
        terrain.fill(ground);
        (void)terrain.update();
        report("Terrain fill and build 513x257, 128 chunks", std::chrono::steady_clock::now() - start, 1);

        math::Random random(7);
        // clang-format off
        measure("Terrain carve radius 24 and update", 200, [&terrain, &random] {
            terrain.carve(math::Circle{{random.range(100.0F, 1948.0F), random.range(300.0F, 900.0F)}, 24.0F});
            (void)terrain.update();
        });
        // clang-format on
    }
};

} // namespace haylen::bench

int main() {
    return haylen::bench::ProceduralBenchmark::run();
}
