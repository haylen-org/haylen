#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "haylen/math/Vec2.hpp"

namespace haylen::math {

// Splits the inside of a shape into triangles. A shape is a list of closed outlines that fill by the nonzero rule: an outline alone fills in either winding, an outline wound opposite to the outline around it cuts a hole, and one wound the same way adds nothing. Every hole joins the outline around it through a bridge to a vertex of that outline it sees, and the joined ring loses one ear after another. The storage stays from shape to shape, so a shape triangulated every frame allocates nothing once it has grown.
class Triangulator final {
  public:
    // What an outline is to the fill: the outer edge of a filled region, the edge of a hole in one, or an outline with the fill on both of its sides or on neither, such as an outline without area.
    enum class Kind : std::uint8_t {
        Outer,
        Hole,
        Inside,
    };

    // Triangulates the shape whose outlines are the runs of `counts` points that follow each other in `points`. The triangles index `points` and turn the way an outline with a positive `Geometry::signedArea` turns. An outline that crosses itself fills only what its ears reach.
    void triangulate(std::span<const Vec2> points, std::span<const std::uint32_t> counts);

    [[nodiscard]] const std::vector<std::uint32_t>& getTriangles() const noexcept {
        return triangles;
    }

    // The kind of every outline, in the order of the counts.
    [[nodiscard]] const std::vector<Kind>& getKinds() const noexcept {
        return kinds;
    }

  private:
    static constexpr std::uint32_t kNone = 0xFFFFFFFFU;

    // How far a point may lie from an edge of an outline, as a share of the size of its coordinates, and still count as on the edge.
    static constexpr float kEdgeTolerance = 1.0e-5F;

    // A vertex of a ring that clips: the point it stands on and its neighbours, which clipping and bridges relink.
    struct Node {
        std::uint32_t point = 0;
        std::uint32_t previous = 0;
        std::uint32_t next = 0;
    };

    // An outline of the shape: where its points start, how many it has, the corners of its bounds and its signed area, the outline right around it, the winding of the fill inside it, and for a hole the outer outline of the region it cuts.
    struct Outline {
        std::uint32_t first = 0;
        std::uint32_t count = 0;
        Vec2 low{};
        Vec2 high{};
        float area = 0.0F;
        std::uint32_t parent = kNone;
        int winding = 0;
        std::uint32_t region = kNone;
    };

    // Where a point lies against an outline.
    enum class Side : std::uint8_t {
        Inside,
        Outside,
        Edge,
    };

    [[nodiscard]] static float turn(Vec2 a, Vec2 b, Vec2 c) noexcept;
    [[nodiscard]] static bool inTriangle(Vec2 a, Vec2 b, Vec2 c, Vec2 point) noexcept;
    [[nodiscard]] static bool crosses(Vec2 firstStart, Vec2 firstEnd, Vec2 secondStart, Vec2 secondEnd) noexcept;
    [[nodiscard]] static bool inBox(Vec2 a, Vec2 point, Vec2 b) noexcept;

    void classify(std::span<const std::uint32_t> counts);
    [[nodiscard]] bool encloses(const Outline& outer, const Outline& inner) const noexcept;
    [[nodiscard]] Side locate(const Outline& outline, Vec2 point) const noexcept;

    [[nodiscard]] std::uint32_t link(const Outline& outline, bool positive);
    [[nodiscard]] std::uint32_t getLeftmost(std::uint32_t start) const noexcept;
    [[nodiscard]] std::uint32_t joinHole(std::uint32_t hole, std::uint32_t ring);
    [[nodiscard]] std::uint32_t findBridge(std::uint32_t hole, std::uint32_t ring) const noexcept;
    [[nodiscard]] std::uint32_t split(std::uint32_t from, std::uint32_t to);
    [[nodiscard]] std::uint32_t filter(std::uint32_t start, std::uint32_t end) noexcept;
    [[nodiscard]] std::uint32_t cureCrossings(std::uint32_t start);
    void clip(std::uint32_t ear);
    [[nodiscard]] bool isEar(std::uint32_t ear) const noexcept;
    [[nodiscard]] bool isLocallyInside(std::uint32_t from, std::uint32_t to) const noexcept;
    [[nodiscard]] bool sectorContains(std::uint32_t outer, std::uint32_t inner) const noexcept;
    void remove(std::uint32_t node) noexcept;
    void emit(std::uint32_t a, std::uint32_t b, std::uint32_t c);

    [[nodiscard]] Vec2 at(std::uint32_t node) const noexcept {
        return points[nodes[node].point];
    }
    [[nodiscard]] float turnAt(std::uint32_t a, std::uint32_t b, std::uint32_t c) const noexcept {
        return turn(at(a), at(b), at(c));
    }

    std::span<const Vec2> points;
    std::vector<Outline> outlines;
    std::vector<Kind> kinds;
    std::vector<std::uint32_t> order;
    std::vector<Node> nodes;
    std::vector<std::uint32_t> holes;
    std::vector<std::uint32_t> triangles;
};

} // namespace haylen::math
