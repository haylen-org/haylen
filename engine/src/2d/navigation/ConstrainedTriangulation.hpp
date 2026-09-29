#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

#include "haylen/math/Segment.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::navigation2d {

// Builds the constrained Delaunay triangulation of a set of segments: the triangulation of their end points closest to Delaunay in which every segment is a triangle edge. Points closer than a tiny tolerance merge, and segments that cross or touch are split where they meet. Only the areas enclosed by the segments that a test accepts are kept, and they are refined with points on the segments where vertices come close to them, so a disk of any size passes a chain of kept triangles exactly when every edge it crosses is at least as long as its diameter. Coordinates run in double precision, the orientation test is exact, and the in-circle test decides only beyond rounding doubt.
class ConstrainedTriangulation final {
  public:
    // Vertices run counterclockwise in a y-up frame, which is clockwise on screen. The neighbor and the constraint flag at index i belong to the edge from vertex i to vertex i + 1.
    struct Triangle {
        std::array<std::int32_t, 3> vertices{};
        std::array<std::int32_t, 3> neighbors{-1, -1, -1};
        std::array<bool, 3> constrained{};
    };

    // Keeps the areas that the test accepts at a point inside them. The neighbor of a kept triangle across the edge of an area is -1.
    void build(std::span<const math::Segment> segments, const std::function<bool(math::Vec2)>& keeps);

    [[nodiscard]] const std::vector<math::Vec2>& getVertices() const noexcept {
        return vertices;
    }
    [[nodiscard]] const std::vector<Triangle>& getTriangles() const noexcept {
        return triangles;
    }

  private:
    struct Point {
        double x = 0.0;
        double y = 0.0;
    };

    using Edge = std::pair<std::int32_t, std::int32_t>;

    // The edge at an index of a triangle.
    struct Side {
        std::int32_t triangle = 0;
        int index = 0;
    };

    // A welded input segment and the points where other segments cut it, by their position along it.
    struct Piece {
        std::int32_t start = 0;
        std::int32_t end = 0;
        std::vector<std::pair<double, std::int32_t>> cuts;
    };

    // The static filters of Shewchuk bound the rounding error of the orientation and in-circle determinants relative to their permanents.
    static constexpr double kOrientErrorBound = (3.0 + 16.0 * 0x1p-53) * 0x1p-53;
    static constexpr double kInCircleErrorBound = (10.0 + 96.0 * 0x1p-53) * 0x1p-53;

    // Returns 1 when a, b and c run counterclockwise, -1 when they run clockwise and 0 when they lie on one line, exactly for any coordinates, so every decision about which side of an edge a point lies on agrees with every other one.
    [[nodiscard]] static int orient(const Point& a, const Point& b, const Point& c) noexcept;
    [[nodiscard]] static int exactOrient(const Point& a, const Point& b, const Point& c) noexcept;

    // Tells whether d lies inside the circle through the counterclockwise a, b and c beyond any rounding doubt. Nearly cocircular points count as outside, so a flip never undoes another and legalizing always ends.
    [[nodiscard]] static bool inCircle(const Point& a, const Point& b, const Point& c, const Point& d) noexcept;

    [[nodiscard]] static double distance(const Point& a, const Point& b) noexcept;

    // Returns the foot of the point on the segment from a to b when it falls strictly between them.
    [[nodiscard]] static std::optional<Point> projectInside(const Point& point, const Point& a, const Point& b) noexcept;

    [[nodiscard]] const Point& pointAt(std::int32_t index) const noexcept {
        return points[static_cast<std::size_t>(index)];
    }

    [[nodiscard]] bool isSuper(std::int32_t vertex) const noexcept {
        return vertex >= firstSuper && vertex < firstSuper + 3;
    }

    // Tells whether the segments from a to b and from c to d cross at a point inside both.
    [[nodiscard]] bool crosses(std::int32_t a, std::int32_t b, std::int32_t c, std::int32_t d) const noexcept;

    // Welds the segment end points and splits segments where they cross or touch, filling points and constraints.
    void prepare(std::span<const math::Segment> segments);
    [[nodiscard]] std::int32_t weld(const Point& point);
    void intersect(std::size_t first, std::size_t second);

    void addSuperTriangle();
    void insertPoint(std::int32_t vertex);
    [[nodiscard]] std::int32_t locate(const Point& point) const;
    void splitTriangle(std::int32_t triangle, std::int32_t vertex, std::vector<std::pair<std::int32_t, int>>& pending);
    void splitEdge(std::int32_t triangle, int side, std::int32_t vertex, std::vector<std::pair<std::int32_t, int>>& pending);
    void setTriangle(std::int32_t index, std::array<std::int32_t, 3> corners, std::array<std::int32_t, 3> around, std::array<bool, 3> fixed);
    void replaceNeighbor(std::int32_t triangle, std::int32_t from, std::int32_t to) noexcept;
    [[nodiscard]] int edgeIndex(std::int32_t triangle, std::int32_t from, std::int32_t to) const noexcept;
    [[nodiscard]] int cornerIndex(std::int32_t triangle, std::int32_t vertex) const noexcept;

    // Finds the triangle that holds the directed edge, or -1 when the edge does not exist.
    [[nodiscard]] std::int32_t findEdge(std::int32_t from, std::int32_t to) const;

    // Replaces the edge shared by a triangle and its neighbor across edge side with the other diagonal of their quad, and returns the two triangles now holding the new edge.
    std::pair<std::int32_t, std::int32_t> flip(std::int32_t triangle, int side);
    void legalize(std::vector<std::pair<std::int32_t, int>>& pending);

    // Fills crossed with the edges the segment between two vertices crosses, each with the vertex on the right of the segment first. Returns false when a vertex lies exactly on the segment, and stores that vertex in onSegment.
    bool collectCrossings(std::int32_t from, std::int32_t to, std::deque<Edge>& crossed, std::int32_t& onSegment) const;
    void insertConstraint(std::int32_t from, std::int32_t to);
    void setConstrained(std::int32_t from, std::int32_t to);

    // Marks the triangles of every area that no segment crosses as kept when the area is enclosed and the test accepts it.
    void classify(const std::function<bool(math::Vec2)>& keeps);

    // Adds points on the constrained edges of the kept areas until no vertex of a kept triangle lies closer to a constrained edge beyond it than the shorter side at that vertex, the refinement of Lens and Boigelot. The points split the constrained edges at the feet of those vertices, so every narrow place between a vertex and a segment becomes a short edge.
    void refine();

    // Splits the constrained edge that the corner of a triangle, or the point mirroring it on the circumcircle, reaches across the far side closer than the shorter side at the corner, at the foot of the corner, and tells whether it did.
    bool refineCorner(std::int32_t triangle, int corner);

    // Walks away from the point across the side and the sides beyond it whose feet from the point lie inside them and nearer than the reach, and returns the constrained side the walk meets.
    [[nodiscard]] std::optional<Side> findConstraint(const Point& point, Side side, double reach) const;
    void finish();

    double tolerance = 0.0;
    std::unordered_map<std::uint64_t, std::int32_t> weldIndex;
    std::vector<Piece> pieces;
    std::vector<Point> points;
    std::vector<Edge> constraints;
    std::vector<Triangle> triangles;
    std::vector<bool> kept;
    std::vector<std::int32_t> vertexTriangles;
    std::vector<math::Vec2> vertices;
    std::int32_t firstSuper = 0;
    mutable std::int32_t lastTriangle = 0;
};

} // namespace haylen::navigation2d
