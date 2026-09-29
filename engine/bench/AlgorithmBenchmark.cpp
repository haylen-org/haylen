#include <chrono>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "haylen/2d/navigation/Crowd.hpp"
#include "haylen/2d/navigation/FlowField.hpp"
#include "haylen/2d/navigation/Grid.hpp"
#include "haylen/2d/navigation/GridSearch.hpp"
#include "haylen/2d/navigation/HierarchicalPathfinder.hpp"
#include "haylen/2d/navigation/NavMesh.hpp"
#include "haylen/2d/physics/RayBatch.hpp"
#include "haylen/2d/physics/Raycaster.hpp"
#include "haylen/2d/physics/World.hpp"
#include "haylen/2d/spatial/AabbTree.hpp"
#include "haylen/2d/spatial/KdTree.hpp"
#include "haylen/2d/spatial/Neighbor.hpp"
#include "haylen/2d/spatial/RayHit.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/math/Random.hpp"
#include "haylen/math/Ray.hpp"
#include "haylen/math/Vec2.hpp"
#include "varn/runtime/Runtime.h"

namespace haylen::bench {

// Times the path finding, crowd, spatial and ray casting algorithms on the CPU and prints the average time of each run.
class AlgorithmBenchmark final {
  public:
    static int run() {
        varn::runtime::Runtime runtime({"haylen-algorithm-benchmark"}, 0);
        core::JobSystem jobs(runtime, [](const std::string& message) { std::fprintf(stderr, "%s\n", message.c_str()); });
        std::printf("%-52s %12s %8s\n", "Algorithm", "Average ms", "Runs");

        measureGridPaths();
        measureNavMesh();
        measureCrowd(jobs);
        measureSpatial();
        measureRaycasts(jobs);
        return 0;
    }

  private:
    static constexpr int kGridSize = 512;
    static constexpr int kPathPairs = 100;

    static void measure(const char* name, int runs, const std::function<void()>& work) {
        work();
        const auto start = std::chrono::steady_clock::now();
        for (int run = 0; run < runs; ++run) {
            work();
        }
        std::printf("%-52s %12.3f %8d\n", name, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / runs, runs);
    }

    // Fills a grid with random walls, keeping every chosen start and goal open so each pair has a real search to do.
    [[nodiscard]] static navigation2d::Grid mazeGrid(float wallChance, std::vector<std::pair<navigation2d::Grid::Cell, navigation2d::Grid::Cell>>& pairs) {
        navigation2d::Grid grid(kGridSize, kGridSize);
        math::Random random(11);
        for (int y = 0; y < kGridSize; ++y) {
            for (int x = 0; x < kGridSize; ++x) {
                grid.setWalkable({x, y}, !random.chance(wallChance));
            }
        }
        for (int pair = 0; pair < kPathPairs; ++pair) {
            const navigation2d::Grid::Cell start{random.range(0, 63), random.range(0, kGridSize - 1)};
            const navigation2d::Grid::Cell goal{random.range(kGridSize - 64, kGridSize - 1), random.range(0, kGridSize - 1)};
            grid.setWalkable(start, true);
            grid.setWalkable(goal, true);
            pairs.emplace_back(start, goal);
        }
        return grid;
    }

    static void measureGridPaths() {
        std::vector<std::pair<navigation2d::Grid::Cell, navigation2d::Grid::Cell>> sparsePairs;
        const navigation2d::Grid sparse = mazeGrid(0.05F, sparsePairs);
        std::vector<std::pair<navigation2d::Grid::Cell, navigation2d::Grid::Cell>> pairs;
        const navigation2d::Grid grid = mazeGrid(0.25F, pairs);
        navigation2d::GridSearch search;
        // clang-format off
        measure("A* 512x512, 5% walls, one path across", kPathPairs, [&, pair = std::size_t{0}]() mutable {
            (void)search.findPath(sparse, sparsePairs[pair].first, sparsePairs[pair].second);
            pair = (pair + 1) % sparsePairs.size();
        });
        measure("Jump point search 512x512, 5% walls", kPathPairs, [&, pair = std::size_t{0}]() mutable {
            (void)search.findPath(sparse, sparsePairs[pair].first, sparsePairs[pair].second, {.jumpPoint = true});
            pair = (pair + 1) % sparsePairs.size();
        });
        measure("A* 512x512, 25% walls, one path across", kPathPairs, [&, pair = std::size_t{0}]() mutable {
            (void)search.findPath(grid, pairs[pair].first, pairs[pair].second);
            pair = (pair + 1) % pairs.size();
        });
        measure("Weighted A* 512x512, weight 2", kPathPairs, [&, pair = std::size_t{0}]() mutable {
            (void)search.findPath(grid, pairs[pair].first, pairs[pair].second, {.weight = 2.0F});
            pair = (pair + 1) % pairs.size();
        });
        measure("Jump point search 512x512, one path across", kPathPairs, [&, pair = std::size_t{0}]() mutable {
            (void)search.findPath(grid, pairs[pair].first, pairs[pair].second, {.jumpPoint = true});
            pair = (pair + 1) % pairs.size();
        });
        measure("Flow field 512x512, 4 goals", 10, [&grid, &pairs] {
            navigation2d::FlowField field;
            field.compute(grid, std::vector<navigation2d::Grid::Cell>{pairs.front().second, {0, 0}, {511, 511}, {256, 256}});
        });
        // clang-format on

        measure("HPA* build 512x512, clusters of 16", 3, [&grid] { navigation2d::HierarchicalPathfinder hierarchy(grid, {.clusterSize = 16}); });
        navigation2d::HierarchicalPathfinder hierarchy(grid, {.clusterSize = 16});
        // clang-format off
        measure("HPA* 512x512, one path across", kPathPairs, [&, pair = std::size_t{0}]() mutable {
            (void)hierarchy.findPath(grid, pairs[pair].first, pairs[pair].second);
            pair = (pair + 1) % pairs.size();
        });
        // clang-format on
    }

    static void measureNavMesh() {
        navigation2d::NavMesh mesh;
        mesh.setBoundary(std::vector<math::Vec2>{{0.0F, 0.0F}, {4096.0F, 0.0F}, {4096.0F, 4096.0F}, {0.0F, 4096.0F}});
        math::Random random(12);
        for (int obstacle = 0; obstacle < 400; ++obstacle) {
            const math::Vec2 center{random.range(100.0F, 3996.0F), random.range(100.0F, 3996.0F)};
            const float size = random.range(10.0F, 60.0F);
            mesh.addObstacle(std::vector<math::Vec2>{center + math::Vec2{-size, -size}, center + math::Vec2{size, -size}, center + math::Vec2{size, size}, center + math::Vec2{-size, size}});
        }
        measure("Navmesh build 4096x4096, 400 obstacles", 5, [&mesh] { mesh.build(); });

        std::vector<std::pair<math::Vec2, math::Vec2>> trips;
        while (trips.size() < 200) {
            const math::Vec2 start{random.range(0.0F, 4096.0F), random.range(0.0F, 4096.0F)};
            const math::Vec2 goal{random.range(0.0F, 4096.0F), random.range(0.0F, 4096.0F)};
            if (mesh.contains(start) && mesh.contains(goal)) {
                trips.emplace_back(start, goal);
            }
        }
        // clang-format off
        measure("Navmesh path with the funnel, radius 8", 1000, [&, trip = std::size_t{0}]() mutable {
            (void)mesh.findPath(trips[trip].first, trips[trip].second, 8.0F);
            trip = (trip + 1) % trips.size();
        });
        // clang-format on
    }

    static void measureCrowd(core::JobSystem& jobs) {
        navigation2d::Crowd crowd;
        math::Random random(13);
        for (int agent = 0; agent < 2000; ++agent) {
            const math::Vec2 position{random.range(0.0F, 3000.0F), random.range(0.0F, 3000.0F)};
            const std::uint32_t id = crowd.addAgent({.position = position, .radius = 8.0F, .maxSpeed = 80.0F, .neighborDistance = 60.0F});
            crowd.setTarget(id, {3000.0F - position.x, 3000.0F - position.y});
        }
        measure("Crowd step, 2000 ORCA agents, one thread", 20, [&crowd] { crowd.step(1.0F / 60.0F); });
        measure("Crowd step, 2000 ORCA agents, job system", 20, [&crowd, &jobs] { crowd.step(1.0F / 60.0F, &jobs); });
    }

    static void measureSpatial() {
        spatial2d::AabbTree tree;
        spatial2d::KdTree points;
        math::Random random(14);
        for (std::uint64_t id = 0; id < 100000; ++id) {
            const math::Vec2 position{random.range(0.0F, 10000.0F), random.range(0.0F, 10000.0F)};
            tree.set(id, {position.x, position.y, 8.0F, 8.0F});
            points.set(id, position);
        }
        points.build();
        std::vector<std::uint64_t> ids;
        std::vector<spatial2d::Neighbor> neighbors;
        std::vector<spatial2d::RayHit> hits;
        // clang-format off
        measure("AABB tree, 1000 area queries in 100000 entries", 10, [&] {
            for (int query = 0; query < 1000; ++query) {
                tree.query({random.range(0.0F, 9900.0F), random.range(0.0F, 9900.0F), 100.0F, 100.0F}, ids);
            }
        });
        measure("AABB tree, 1000 rays of 500 units", 10, [&] {
            for (int ray = 0; ray < 1000; ++ray) {
                const math::Vec2 start{random.range(0.0F, 10000.0F), random.range(0.0F, 10000.0F)};
                tree.raycast(math::Ray::fromAngle(start, random.range(0.0F, 6.28F), 500.0F), 1, hits);
            }
        });
        measure("k-d tree, 1000 queries for the 8 nearest", 10, [&] {
            for (int query = 0; query < 1000; ++query) {
                points.nearest({random.range(0.0F, 10000.0F), random.range(0.0F, 10000.0F)}, 8, 1e9F, neighbors);
            }
        });
        // clang-format on
    }

    static void measureRaycasts(core::JobSystem& jobs) {
        physics2d::World world({.gravity = {}});
        math::Random random(15);
        for (int box = 0; box < 2000; ++box) {
            physics2d::Body body = world.createBody({.type = physics2d::Body::Type::Static, .position = {random.range(0.0F, 4000.0F), random.range(0.0F, 4000.0F)}});
            body.addBox({random.range(10.0F, 60.0F), random.range(10.0F, 60.0F)});
        }
        physics2d::RayBatch batch;
        batch.resize(100000);
        for (std::size_t ray = 0; ray < batch.size(); ++ray) {
            const math::Vec2 start{random.range(0.0F, 4000.0F), random.range(0.0F, 4000.0F)};
            batch.setRay(ray, start, start + math::Vec2::fromAngle(random.range(0.0F, 6.28F), 400.0F));
        }

        const physics2d::Raycaster caster(world);
        measure("Physics batch of 100000 rays, one thread", 5, [&] { caster.castBatch(batch); });
        measure("Physics batch of 100000 rays, job system", 5, [&] { caster.castBatch(batch, {}, &jobs); });
        std::printf("Job system workers: %zu, hardware threads: %u\n", jobs.getWorkerCount(), std::thread::hardware_concurrency());
    }
};

} // namespace haylen::bench

int main() {
    return haylen::bench::AlgorithmBenchmark::run();
}
