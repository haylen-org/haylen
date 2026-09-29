#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <vector>

#include "haylen/2d/spatial/AabbTree.hpp"
#include "haylen/math/Segment.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::navigation2d {

// A navigation mesh of triangles over the walkable area of a level, built with a constrained Delaunay triangulation from a boundary polygon and obstacle polygons, such as the collision objects of a Tiled map or generated shapes. Obstacles may overlap each other and cross the boundary. Paths run through the triangles with A* and are pulled straight with the funnel algorithm, keeping an agent radius clear of the corners they turn around. Changing the polygons marks the mesh for a rebuild, which happens on the next query or on build.
class NavMesh final {
  public:
    // Vertices run counterclockwise in a y-up frame, which is clockwise on screen. The neighbor at index i shares the edge from vertex i to vertex i + 1, and -1 marks a wall.
    struct Triangle {
        std::array<std::uint32_t, 3> vertices{};
        std::array<std::int32_t, 3> neighbors{-1, -1, -1};
    };

    // Sets the outline of the walkable area, which needs at least three points.
    void setBoundary(std::span<const math::Vec2> polygon);
    [[nodiscard]] const std::vector<math::Vec2>& getBoundary() const noexcept {
        return boundary;
    }

    // Obstacles are polygons of at least three points, identified by the ids addObstacle returns.
    std::uint32_t addObstacle(std::span<const math::Vec2> polygon);
    void setObstacle(std::uint32_t id, std::span<const math::Vec2> polygon);
    bool removeObstacle(std::uint32_t id);
    void clearObstacles() noexcept;
    [[nodiscard]] std::size_t getObstacleCount() const noexcept {
        return obstacles.size();
    }

    // Tells whether the polygons changed since the last build.
    [[nodiscard]] bool isDirty() const noexcept {
        return dirty;
    }
    void build();

    // Returns the start, the corners to turn at and the goal, or an empty path when either point lies outside the mesh or no corridor is wide enough for the agent. The path stays valid until the next search.
    std::span<const math::Vec2> findPath(math::Vec2 start, math::Vec2 goal, float agentRadius = 0.0F);

    // Returns the length of the last path found, which is infinite when there was none.
    [[nodiscard]] float getPathLength() const noexcept {
        return pathLength;
    }

    std::optional<std::size_t> findTriangle(math::Vec2 point);
    bool contains(math::Vec2 point);

    // Returns the point itself inside the mesh and the closest point of its edge outside it, or nothing for an empty mesh.
    std::optional<math::Vec2> getClosestPoint(math::Vec2 point);

    const std::vector<math::Vec2>& getVertices();
    const std::vector<Triangle>& getTriangles();

  private:
    struct OpenNode {
        float estimate = 0.0F;
        std::int32_t triangle = 0;
    };

    struct Portal {
        math::Vec2 left{};
        math::Vec2 right{};
    };

    // Points closer than this to a triangle still count as inside it.
    static constexpr float kEdgeTolerance = 1e-3F;

    [[nodiscard]] static bool isWorse(const OpenNode& lhs, const OpenNode& rhs) noexcept;

    // Returns twice the signed area of the triangle a, b, c in the orientation the funnel algorithm uses.
    [[nodiscard]] static float area(math::Vec2 a, math::Vec2 b, math::Vec2 c) noexcept;
    static void requirePolygon(std::span<const math::Vec2> polygon);
    void ensureBuilt();
    [[nodiscard]] bool isInside(std::size_t triangle, math::Vec2 point) const noexcept;
    [[nodiscard]] math::Vec2 vertexAt(std::uint32_t index) const noexcept {
        return vertices[index];
    }

    // Returns how wide the way through a triangle is between the edge it enters by and the edge it leaves by, the width of Demyen and Buro: the distance from their shared corner to the third edge when that edge is a wall, and otherwise the shorter of the two edges.
    [[nodiscard]] float passageWidth(const Triangle& triangle, std::size_t entry, std::size_t exit) const noexcept;

    // Finds the corridor of triangles from the start to the goal triangle with A* over their shared edges, skipping edges and passages narrower than the agent.
    bool findCorridor(std::int32_t start, std::int32_t goal, math::Vec2 from, math::Vec2 to, float agentRadius);
    void pullString(math::Vec2 start, math::Vec2 goal, float agentRadius);

    std::vector<math::Vec2> boundary;
    std::map<std::uint32_t, std::vector<math::Vec2>> obstacles;
    std::uint32_t nextObstacle = 1;
    bool dirty = true;

    std::vector<math::Segment> segments;
    std::vector<math::Vec2> vertices;
    std::vector<Triangle> triangles;
    std::vector<math::Segment> walls;
    spatial2d::AabbTree triangleIndex;
    std::vector<std::uint64_t> candidates;

    std::vector<float> scores;
    std::vector<std::int32_t> parents;
    std::vector<std::int32_t> entrySides;
    std::vector<math::Vec2> entries;
    std::vector<std::uint32_t> visits;
    std::uint32_t visit = 0;
    std::vector<OpenNode> open;
    std::vector<std::int32_t> corridor;
    std::vector<Portal> portals;
    std::vector<math::Vec2> path;
    float pathLength = 0.0F;
};

} // namespace haylen::navigation2d
