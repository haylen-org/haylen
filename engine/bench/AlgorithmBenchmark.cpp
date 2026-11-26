#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <optional>
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
#include "haylen/3d/spatial/AabbTree.hpp"
#include "haylen/3d/spatial/Bvh.hpp"
#include "haylen/3d/spatial/HashGrid.hpp"
#include "haylen/3d/spatial/KdTree.hpp"
#include "haylen/3d/spatial/MeshBvh.hpp"
#include "haylen/3d/spatial/Octree.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/math/Box.hpp"
#include "haylen/math/Frustum.hpp"
#include "haylen/math/Geometry3D.hpp"
#include "haylen/math/Mat4.hpp"
#include "haylen/math/MatrixBatch.hpp"
#include "haylen/math/Quat.hpp"
#include "haylen/math/Random.hpp"
#include "haylen/math/Ray2D.hpp"
#include "haylen/math/Ray3D.hpp"
#include "haylen/math/Sphere.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::bench {

// Times the path finding, crowd, spatial structures of 2D and 3D, mesh hierarchy, ray casting and batch math algorithms on the CPU and prints the average time of each run.
class AlgorithmBenchmark final {
  public:
    static int run() {
        core::JobSystem jobs([](const std::string& message) { std::fprintf(stderr, "%s\n", message.c_str()); });
        std::printf("%-52s %12s %8s\n", "Algorithm", "Average ms", "Runs");

        measureGridPaths();
        measureNavMesh();
        measureCrowd(jobs);
        measureSpatial();
        measureSpatial3D();
        measureMeshBvh();
        measureRaycasts(jobs);
        measureBatchMath();
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

    // Times runs without a warm-up, since the first run matters, and prints the slowest run next to the average.
    static void measureCold(const char* name, int runs, const std::function<void()>& work) {
        double total = 0.0;
        double slowest = 0.0;
        for (int run = 0; run < runs; ++run) {
            const auto start = std::chrono::steady_clock::now();
            work();
            const double milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            total += milliseconds;
            slowest = std::max(slowest, milliseconds);
        }

        std::printf("%-52s %12.3f %8d\n", name, total / runs, runs);
        std::printf("%-52s %12.3f %8d\n", "  slowest run", slowest, 1);
    }

    // Returns a navmesh of 4096 by 4096 units with 400 square obstacles, before it is built.
    [[nodiscard]] static navigation2d::NavMesh obstacleMesh(math::Random& random) {
        navigation2d::NavMesh mesh;
        mesh.setBoundary(std::vector<math::Vec2>{{0.0F, 0.0F}, {4096.0F, 0.0F}, {4096.0F, 4096.0F}, {0.0F, 4096.0F}});
        for (int obstacle = 0; obstacle < 400; ++obstacle) {
            const math::Vec2 center{random.range(100.0F, 3996.0F), random.range(100.0F, 3996.0F)};
            const float size = random.range(10.0F, 60.0F);
            mesh.addObstacle(std::vector<math::Vec2>{center + math::Vec2{-size, -size}, center + math::Vec2{size, -size}, center + math::Vec2{size, size}, center + math::Vec2{-size, size}});
        }

        return mesh;
    }

    static void measureNavMesh() {
        math::Random random(12);
        navigation2d::NavMesh mesh = obstacleMesh(random);
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
            const navigation2d::Crowd::AgentId id = crowd.addAgent({.position = position, .radius = 8.0F, .maxSpeed = 80.0F, .neighborDistance = 60.0F});
            crowd.setTarget(id, {3000.0F - position.x, 3000.0F - position.y});
        }

        measure("Crowd step, 2000 ORCA agents, one thread", 20, [&crowd] { crowd.step(1.0F / 60.0F); });
        measure("Crowd step, 2000 ORCA agents, job system", 20, [&crowd, &jobs] { crowd.step(1.0F / 60.0F, &jobs); });

        // Navmesh builds keep every worker busy, like a level that builds its mesh in the background while its crowd walks.
        navigation2d::NavMesh mesh = obstacleMesh(random);
        std::atomic<std::size_t> building = jobs.getWorkerCount() * 4;
        for (std::size_t build = building; build > 0; --build) {
            // clang-format off
            jobs.post([mesh, &building]() mutable {
                mesh.build();
                --building;
            });
            // clang-format on
        }

        measureCold("Crowd step, 2000 ORCA agents, pool busy building", 20, [&crowd, &jobs] { crowd.step(1.0F / 60.0F, &jobs); });

        while (building > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
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
                tree.raycast(math::Ray2D::fromAngle(start, random.range(0.0F, 6.28F), 500.0F), 1, hits);
            }
        });
        measure("k-d tree, 1000 queries for the 8 nearest", 10, [&] {
            for (int query = 0; query < 1000; ++query) {
                points.nearest({random.range(0.0F, 10000.0F), random.range(0.0F, 10000.0F)}, 8, 1e9F, neighbors);
            }
        });
        // clang-format on
    }

    // The queries that every 3D structure answers on one scene, so their times compare with each other and with a search through every box.
    struct SpatialQueries {
        std::vector<math::Box> areas;
        std::vector<math::Frustum> frustums;
        std::vector<math::Ray3D> rays;
        std::vector<math::Vec3> points;
    };

    [[nodiscard]] static SpatialQueries spatialQueries(math::Random& random) {
        SpatialQueries queries;
        for (int query = 0; query < 1000; ++query) {
            const math::Vec3 place{random.range(-500.0F, 500.0F), random.range(-50.0F, 50.0F), random.range(-500.0F, 500.0F)};
            const math::Vec3 aim{random.range(-500.0F, 500.0F), 0.0F, random.range(-500.0F, 500.0F)};
            queries.areas.push_back(math::Box::fromCenter(place, {20.0F, 20.0F, 20.0F}));
            queries.rays.push_back(math::Ray3D::between(place, aim));
            queries.points.push_back(place);
            if (query < 100) {
                const math::Mat4 viewProjection = math::Mat4::perspective({.aspect = 16.0F / 9.0F, .nearPlane = 0.1F, .farPlane = 300.0F}) * math::Mat4::lookAt(place, aim);
                queries.frustums.push_back(math::Frustum::fromMatrix(viewProjection, math::Mat4::DepthRange::ZeroToOne));
            }
        }

        return queries;
    }

    template <typename Structure> static void measureStructure(const char* name, const Structure& structure, const SpatialQueries& queries) {
        std::vector<std::uint64_t> ids;
        std::vector<spatial3d::RayHit> hits;
        std::vector<spatial3d::Neighbor> neighbors;
        const std::string prefix = name;
        // clang-format off
        measure((prefix + ", 1000 box queries").c_str(), 5, [&] {
            for (const math::Box& area : queries.areas) {
                structure.query(area, ids);
            }
        });
        measure((prefix + ", 100 frustum queries").c_str(), 5, [&] {
            for (const math::Frustum& frustum : queries.frustums) {
                structure.queryFrustum(frustum, ids);
            }
        });
        measure((prefix + ", 1000 rays for the first hit").c_str(), 5, [&] {
            for (const math::Ray3D& ray : queries.rays) {
                structure.raycast(ray, 1, hits);
            }
        });
        measure((prefix + ", 1000 queries for the 8 nearest").c_str(), 5, [&] {
            for (const math::Vec3& point : queries.points) {
                structure.nearest(point, 8, 1e9F, neighbors);
            }
        });
        // clang-format on
    }

    // Measures every 3D structure over 100000 boxes of one to four meters in a kilometer, next to a search through every box.
    static void measureSpatial3D() {
        constexpr std::size_t kCount = 100000;
        math::Random random(31);
        std::vector<std::uint64_t> ids;
        std::vector<math::Box> boxes;
        std::vector<math::Sphere> spheres;
        for (std::size_t index = 0; index < kCount; ++index) {
            const math::Vec3 center{random.range(-500.0F, 500.0F), random.range(-50.0F, 50.0F), random.range(-500.0F, 500.0F)};
            const math::Vec3 size{random.range(1.0F, 4.0F), random.range(1.0F, 4.0F), random.range(1.0F, 4.0F)};
            ids.push_back(index);
            boxes.push_back(math::Box::fromCenter(center, size));
            spheres.push_back({center, size.x * 0.5F});
        }

        const SpatialQueries queries = spatialQueries(random);
        spatial3d::HashGrid grid(4.0F);
        spatial3d::Octree octree({{-512.0F, -64.0F, -512.0F}, {512.0F, 64.0F, 512.0F}}, {.maxEntries = 16, .maxDepth = 10});
        spatial3d::AabbTree tree;
        spatial3d::Bvh bvh;
        spatial3d::KdTree points;
        // clang-format off
        measureCold("3D hash grid, set 100000 boxes", 1, [&] { grid.setAll(ids, boxes); });
        measureCold("3D octree, set 100000 boxes", 1, [&] { octree.setAll(ids, boxes); });
        measureCold("3D AABB tree, set 100000 boxes", 1, [&] { tree.setAll(ids, boxes); });
        measureCold("BVH, set and build 100000 boxes", 1, [&] {
            bvh.setAll(ids, boxes);
            bvh.build();
        });
        measure("BVH, refit 100000 boxes", 5, [&] { bvh.refit(); });
        measureCold("3D k-d tree, set and build 100000 spheres", 1, [&] {
            points.setAll(ids, spheres);
            points.build();
        });
        // clang-format on

        measureStructure("3D hash grid", grid, queries);
        measureStructure("3D octree", octree, queries);
        measureStructure("3D AABB tree", tree, queries);
        measureStructure("BVH", bvh, queries);
        measureStructure("3D k-d tree", points, queries);

        std::size_t found = 0;
        // clang-format off
        measure("Every box, 10 box queries", 3, [&] {
            for (std::size_t query = 0; query < 10; ++query) {
                found = static_cast<std::size_t>(std::ranges::count_if(boxes, [&](const math::Box& box) { return box.intersects(queries.areas[query]); }));
            }
        });
        // clang-format on
        std::printf("Boxes in the last area: %zu\n", found);
    }

    // Measures the rays and closest points of a mesh of 200000 triangles, next to a search through every triangle.
    static void measureMeshBvh() {
        constexpr int kSide = 316;
        math::Random random(32);
        std::vector<float> positions;
        for (int z = 0; z <= kSide; ++z) {
            for (int x = 0; x <= kSide; ++x) {
                positions.insert(positions.end(), {static_cast<float>(x), random.range(-0.5F, 0.5F), static_cast<float>(-z)});
            }
        }

        std::vector<std::uint32_t> indices;
        for (int z = 0; z < kSide; ++z) {
            for (int x = 0; x < kSide; ++x) {
                const auto corner = static_cast<std::uint32_t>(z * (kSide + 1) + x);
                const auto row = static_cast<std::uint32_t>(kSide + 1);
                indices.insert(indices.end(), {corner, corner + 1, corner + row + 1, corner, corner + row + 1, corner + row});
            }
        }

        std::optional<spatial3d::MeshBvh> mesh;
        measureCold("Mesh BVH, build 200000 triangles", 1, [&] { mesh.emplace(positions, indices); });
        const auto side = static_cast<float>(kSide);
        std::vector<math::Ray3D> rays;
        for (int ray = 0; ray < 10000; ++ray) {
            const math::Vec3 start{random.range(0.0F, side), 20.0F, random.range(-side, 0.0F)};
            const math::Vec3 end{random.range(0.0F, side), -1.0F, random.range(-side, 0.0F)};
            rays.push_back(math::Ray3D::between(start, end));
        }

        std::size_t hits = 0;
        // clang-format off
        measure("Mesh BVH, 10000 rays", 5, [&] {
            hits = 0;
            for (const math::Ray3D& ray : rays) {
                hits += mesh->raycast(ray).has_value() ? std::size_t{1} : std::size_t{0};
            }
        });
        measure("Mesh BVH, 10000 closest points", 5, [&] {
            for (const math::Ray3D& ray : rays) {
                (void)mesh->closestPoint(ray.origin);
            }
        });
        measure("Every triangle, 10 rays", 3, [&] {
            for (std::size_t ray = 0; ray < 10; ++ray) {
                for (std::uint32_t triangle = 0; triangle < mesh->getTriangleCount(); ++triangle) {
                    (void)math::Geometry3D::raycast(rays[ray], mesh->getTriangle(triangle));
                }
            }
        });
        // clang-format on
        std::printf("Rays that hit the mesh: %zu of %zu\n", hits, rays.size());
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

    // Compares the batches of matrices, points and culling with the same work done one value at a time by the scalar math of `Mat4` and `Frustum`.
    static void measureBatchMath() {
        constexpr std::size_t kCount = 100000;
        math::Random random(23);
        std::vector<math::Mat4> matrices;
        std::vector<math::Vec3> points;
        std::vector<math::Box> boxes;
        for (std::size_t index = 0; index < kCount; ++index) {
            const math::Vec3 position{random.range(-500.0F, 500.0F), random.range(-50.0F, 50.0F), random.range(-500.0F, 500.0F)};
            matrices.push_back(math::Mat4::compose(position, math::Quat::fromAxisAngle(math::Vec3::up(), random.range(0.0F, 6.28F)), {1.0F, 1.0F, 1.0F}));
            points.push_back(position);
            boxes.push_back(math::Box::fromCenter(position, {random.range(0.5F, 8.0F), random.range(0.5F, 8.0F), random.range(0.5F, 8.0F)}));
        }

        const math::Mat4 parent = math::Mat4::compose({10.0F, 0.0F, -5.0F}, math::Quat::fromAxisAngle(math::Vec3::up(), 0.4F), {2.0F, 2.0F, 2.0F});
        const math::Mat4 viewProjection = math::Mat4::perspective({.aspect = 16.0F / 9.0F, .farPlane = 400.0F}) * math::Mat4::lookAt({0.0F, 20.0F, 0.0F}, {100.0F, 0.0F, -100.0F});
        const math::Frustum frustum = math::Frustum::fromMatrix(viewProjection, math::Mat4::DepthRange::ZeroToOne);
        std::vector<math::Mat4> worlds(kCount);
        std::vector<math::Vec3> mapped(kCount);
        std::vector<std::uint32_t> visible(kCount);
        std::size_t kept = 0;

        std::printf("Batch math instructions: %.*s\n", static_cast<int>(math::MatrixBatch::getInstructions().size()), math::MatrixBatch::getInstructions().data());
        // clang-format off
        measure("Parent times 100000 matrices, scalar", 20, [&] {
            for (std::size_t index = 0; index < kCount; ++index) {
                worlds[index] = parent * matrices[index];
            }
        });
        measure("Parent times 100000 matrices, batch", 20, [&] { math::MatrixBatch::multiply(parent, matrices, worlds); });
        measure("Apply a matrix to 100000 points, scalar", 20, [&] {
            for (std::size_t index = 0; index < kCount; ++index) {
                mapped[index] = viewProjection.apply(points[index]);
            }
        });
        measure("Apply a matrix to 100000 points, batch", 20, [&] { math::MatrixBatch::transformPoints(viewProjection, points, mapped); });
        measure("Cull 100000 boxes, scalar", 20, [&] {
            kept = 0;
            for (std::size_t index = 0; index < kCount; ++index) {
                if (frustum.classify(boxes[index]) != math::Frustum::Containment::Outside) {
                    visible[kept] = static_cast<std::uint32_t>(index);
                    ++kept;
                }
            }
        });
        measure("Cull 100000 boxes, batch", 20, [&] { kept = frustum.cull(boxes, visible); });
        // clang-format on
        std::printf("Boxes inside the frustum: %zu of %zu\n", kept, kCount);
    }
};

} // namespace haylen::bench

int main() {
    return haylen::bench::AlgorithmBenchmark::run();
}
