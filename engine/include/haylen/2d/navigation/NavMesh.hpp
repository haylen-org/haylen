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

// A navigation mesh of triangles over the walkable area of a level, built with a constrained Delaunay triangulation from a boundary polygon and obstacle polygons, such as the collision objects of a Tiled map or generated shapes. Obstacles may overlap each other and cross the boundary. The triangulation gains points on the walls where corners come close to them, so a corridor of triangles has room for an agent exactly when every edge it crosses is at least as long as the agent is wide. Paths run through the corridors with A* and are pulled straight with the funnel algorithm around circles of the agent radius at the corners they turn around. Changing the polygons marks the mesh for a rebuild, which happens on the next query or on build.
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

    // Returns the start, the points to turn at and the goal, or an empty path when either point lies outside the mesh or no corridor is wide enough for the agent. With a radius, every leg stays inside the mesh, and the path turns at points off each corner it turns around that keep the legs on both sides of the corner at least the radius away from it. A start closer than the radius to a wall first steps straight to its room, the closest point within twice the radius that is the radius away from every wall, and a goal that close to a wall is reached last from its room in the same way. There is no path when the start or the goal has no room or the straight step to it leaves the mesh. The path stays valid until the next search.
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

    // A shared edge seen from the triangle before it, with its ends narrowed by the agent radius and the corners they came from.
    struct Portal {
        math::Vec2 left{};
        math::Vec2 right{};
        math::Vec2 leftCorner{};
        math::Vec2 rightCorner{};
    };

    // A corner the path turns around, on its left with side 1 and on its right with side -1.
    struct Turn {
        math::Vec2 corner{};
        float side = 0.0F;
    };

    // Points closer than this to a triangle still count as inside it.
    static constexpr float kEdgeTolerance = 1e-3F;

    [[nodiscard]] static bool isWorse(const OpenNode& lhs, const OpenNode& rhs) noexcept;

    // Returns twice the signed area of the triangle a, b, c in the orientation the funnel algorithm uses.
    [[nodiscard]] static float area(math::Vec2 a, math::Vec2 b, math::Vec2 c) noexcept;
    static void requirePolygon(std::span<const math::Vec2> polygon);

    // Returns the direction of the line from the point that passes the target at the signed offset, positive on its left, turning a right angle when the target is closer than the offset.
    [[nodiscard]] static math::Vec2 tangentDirection(math::Vec2 from, math::Vec2 to, float offset) noexcept;

    // These add the points where a segment or a second circle crosses the circle of the radius around the center.
    static void addCrossings(const math::Segment& piece, math::Vec2 center, float radius, std::vector<math::Vec2>& crossings);
    static void addCrossings(math::Vec2 center, math::Vec2 other, float radius, std::vector<math::Vec2>& crossings);

    void ensureBuilt();
    [[nodiscard]] bool isInside(std::size_t triangle, math::Vec2 point) const noexcept;
    [[nodiscard]] math::Vec2 vertexAt(std::uint32_t index) const noexcept {
        return vertices[index];
    }

    // Returns the point itself when it is the radius away from every wall, otherwise its room as findPath describes it, or nothing when the point lies outside the mesh, has no room or cannot step straight to it.
    std::optional<math::Vec2> findRoom(math::Vec2 point, float agentRadius);

    // Gathers the walls of the triangles closer than the distance to the point.
    void gatherWalls(math::Vec2 point, float distance);

    // Tells whether the spot lies at least the radius away from every gathered wall.
    [[nodiscard]] bool hasRoom(math::Vec2 spot, float agentRadius) const noexcept;

    // Tells whether the straight leg between two points crosses no gathered wall away from its start, which may lie on a wall.
    [[nodiscard]] bool canStep(math::Vec2 from, math::Vec2 to) const noexcept;

    // Finds the corridor of triangles from the start to the goal triangle with A* over their shared edges, skipping edges shorter than the agent is wide, which the refined triangulation makes the exact test of room.
    bool findCorridor(std::int32_t start, std::int32_t goal, math::Vec2 from, math::Vec2 to, float agentRadius);

    // Finds the corners the path turns around with the funnel algorithm, over portals narrowed by the agent radius.
    void pullString(math::Vec2 start, math::Vec2 goal, float agentRadius);

    // Extends the path to the goal along the lines that touch a circle of the agent radius around every turn, turning at the corners of polygons drawn around those circles.
    void bendAroundTurns(math::Vec2 start, math::Vec2 goal, float agentRadius);

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
    std::vector<math::Segment> nearWalls;
    std::vector<math::Segment> offsets;
    std::vector<math::Vec2> spots;

    std::vector<float> scores;
    std::vector<std::int32_t> parents;
    std::vector<math::Vec2> entries;
    std::vector<std::uint32_t> visits;
    std::uint32_t visit = 0;
    std::vector<OpenNode> open;
    std::vector<std::int32_t> corridor;
    std::vector<Portal> portals;
    std::vector<Turn> turns;
    std::vector<math::Vec2> directions;
    std::vector<math::Vec2> path;
    float pathLength = 0.0F;
};

} // namespace haylen::navigation2d
