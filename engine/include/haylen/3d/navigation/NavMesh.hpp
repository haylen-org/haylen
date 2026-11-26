#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/3d/navigation/BuildSettings.hpp"
#include "haylen/3d/navigation/Link.hpp"
#include "haylen/3d/navigation/Obstacle.hpp"
#include "haylen/3d/navigation/QueryFilter.hpp"
#include "haylen/3d/navigation/SourceGeometry.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/math/Box.hpp"
#include "haylen/math/Random.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::navigation3d {

class Crowd;

// A navigation mesh of 3D: the walkable surface of a world as convex polygons in tiles, baked from a `SourceGeometry` for one size of agent, with the off-mesh links that cross its gaps and the obstacles that carve it while they stand. Queries find paths, the nearest point, rays along the surface and random points, in meters, on the frame thread. Bakes and the rebuilds of tiles run on the task pool, and their tiles take their place on the frame thread in `update`, so a query never waits for a bake and never sees half of one. A mesh moves but does not copy.
class NavMesh final {
  public:
    // A polygon of the mesh. A reference stays valid until its tile rebuilds.
    using PolygonRef = std::uint64_t;

    // Where a query searches and what it may cross. Points within `searchExtents` of the mesh, half the size of a box around them, snap to it. A path search visits at most `maxNodes` polygons and keeps at most `maxPolygons` of them and `maxPoints` points, and a search that runs out of any of them answers with the partial path it found.
    struct QueryOptions {
        const QueryFilter* filter = nullptr;
        math::Vec3 searchExtents{2.0F, 4.0F, 2.0F};
        std::size_t maxNodes = 4096;
        std::size_t maxPolygons = 1024;
        std::size_t maxPoints = 256;
    };

    // A point on the mesh, its polygon and the area of that polygon.
    struct Location {
        math::Vec3 position;
        PolygonRef polygon = 0;
        std::uint8_t area = 0;
    };

    // The straight path from the start to the goal along the corridor of polygons, turning only at corners. The item of `links` at a point is the link that starts there, or 0. A partial path ends at the point of the mesh closest to the goal that the start reaches, because the goal is cut off or the search ran out.
    struct Path {
        std::vector<math::Vec3> points;
        std::vector<std::uint32_t> links;
        bool partial = false;
        float length = 0.0F;
    };

    // Where a ray along the surface stopped. A ray that reaches its end hits nothing, and a ray that meets a wall gives the fraction of the way it went, the point and the normal of the wall on the ground.
    struct RayHit {
        bool hit = false;
        float fraction = 1.0F;
        math::Vec3 position;
        math::Vec3 normal;
    };

    // The closest wall within the radius of a point, with its distance, the closest point on it and the direction from it toward the point.
    struct WallHit {
        float distance = 0.0F;
        math::Vec3 position;
        math::Vec3 normal;
    };

    // The triangles of the surface of the mesh, three corners each with its area, such as to draw the mesh.
    struct Triangles {
        std::vector<math::Vec3> corners;
        std::vector<std::uint8_t> areas;
    };

    using BuildCallback = std::function<void(core::JobSystem::Result<NavMesh>)>;

    NavMesh(NavMesh&& other) noexcept;
    NavMesh& operator=(NavMesh&& other) noexcept;
    ~NavMesh();

    // Bakes the geometry with the settings on the calling thread. Throws `std::invalid_argument` for settings out of their range, geometry without triangles or a tile whose polygons do not fit in one tile.
    [[nodiscard]] static NavMesh build(const SourceGeometry& source, const BuildSettings& settings);

    // Bakes every tile in a job of its own on the task pool and calls back on the frame thread with the mesh or the error. Settings out of their range throw at once.
    static void buildAsync(core::JobSystem& jobs, std::shared_ptr<const SourceGeometry> source, const BuildSettings& settings, BuildCallback callback);

    // Writes the mesh as the bytes of a `.navmesh` file, which `decode` reads back to the same mesh. Obstacles stay out of the file, since they belong to the running app.
    [[nodiscard]] std::vector<std::uint8_t> encode() const;

    // Reads the bytes of a `.navmesh` file. Throws `std::invalid_argument` with a sentence that names the file for bytes that are damaged or were not written by this version of Haylen.
    [[nodiscard]] static NavMesh decode(std::span<const std::uint8_t> bytes, std::string_view file);

    [[nodiscard]] const BuildSettings& getSettings() const noexcept;
    [[nodiscard]] const math::Box& getBounds() const noexcept;
    [[nodiscard]] std::size_t getTileCount() const noexcept;
    [[nodiscard]] std::size_t getPolygonCount() const noexcept;

    // The names of the areas from area 1 on, and the area of a name. Throws `std::invalid_argument` for a name that the mesh does not hold.
    [[nodiscard]] const std::vector<std::string>& getAreas() const noexcept;
    [[nodiscard]] std::uint8_t getArea(std::string_view name) const;

    [[nodiscard]] std::optional<Location> findNearest(math::Vec3 point, const QueryOptions& options = kDefaultQuery) const;

    // Returns nothing when the start or the goal is not within the search extents of the mesh.
    [[nodiscard]] std::optional<Path> findPath(math::Vec3 start, math::Vec3 goal, const QueryOptions& options = kDefaultQuery) const;

    // Casts a ray along the surface from the start, which must lie on the mesh, toward the end seen from above. Returns nothing when the start is off the mesh.
    [[nodiscard]] std::optional<RayHit> raycast(math::Vec3 start, math::Vec3 end, const QueryOptions& options = kDefaultQuery) const;

    // Moves from the start toward the end along the surface, sliding along the walls it meets, and returns where it stopped, or nothing when the start is off the mesh.
    [[nodiscard]] std::optional<Location> moveAlongSurface(math::Vec3 start, math::Vec3 end, const QueryOptions& options = kDefaultQuery) const;

    [[nodiscard]] std::optional<WallHit> findDistanceToWall(math::Vec3 point, float radius, const QueryOptions& options = kDefaultQuery) const;

    // Returns a point of the mesh where every square meter is as likely as any other, or one within the radius of the center that the center reaches, or nothing for a mesh without polygons the filter passes.
    [[nodiscard]] std::optional<Location> findRandomPoint(math::Random& random, const QueryOptions& options = kDefaultQuery) const;
    [[nodiscard]] std::optional<Location> findRandomPointAround(math::Vec3 center, float radius, math::Random& random, const QueryOptions& options = kDefaultQuery) const;

    // Obstacles need a mesh baked with `obstacles`. Their tiles rebuild in the next `update`. Throws `std::invalid_argument` on a mesh without obstacles, for an obstacle beyond its limit or for sizes that are not finite numbers above zero.
    std::uint32_t addObstacle(const Obstacle& obstacle);
    void setObstacle(std::uint32_t id, const Obstacle& obstacle);
    bool removeObstacle(std::uint32_t id);
    [[nodiscard]] std::optional<Obstacle> getObstacle(std::uint32_t id) const;
    [[nodiscard]] std::size_t getObstacleCount() const noexcept;

    // Links that the source of the bake held keep the ids from 1 on in their order. A new link, or one removed, rebuilds the tile of its start in the next `update`.
    std::uint32_t addLink(const Link& link);
    bool removeLink(std::uint32_t id);
    [[nodiscard]] std::optional<Link> getLink(std::uint32_t id) const;
    [[nodiscard]] std::size_t getLinkCount() const noexcept;

    // Bakes the tiles that touch the bounds again from the geometry in the next `update`, such as after a wall fell. The links of the geometry stay out, since links belong to the mesh. Throws `std::invalid_argument` for bounds outside the bounds the mesh was baked for.
    void rebuild(std::shared_ptr<const SourceGeometry> source, const math::Box& area);

    // Starts the rebuild of every tile that changed, each in a job of its own on the task pool, or on the calling thread without jobs, and puts the tiles that finished in place, which ends the polygon references of the tiles they replace.
    void update(core::JobSystem* jobs);

    // Calls back on the frame thread once no tile waits for its rebuild, at once when none does, with an empty message or the error of a rebuild that failed. A rebuild that fails while nothing waits throws its error on the frame thread, where it becomes an error of the engine.
    void whenUpToDate(std::function<void(std::string)> callback);
    [[nodiscard]] bool isUpToDate() const noexcept;
    [[nodiscard]] std::size_t getPendingTileCount() const noexcept;

    [[nodiscard]] Triangles getTriangles() const;

    // The options of a query that names none.
    static const QueryOptions kDefaultQuery;

  private:
    struct State;

    explicit NavMesh(std::unique_ptr<State> value);

    void requireObstacle(const Obstacle& obstacle) const;

    friend class Crowd;

    std::shared_ptr<State> state;
};

} // namespace haylen::navigation3d
