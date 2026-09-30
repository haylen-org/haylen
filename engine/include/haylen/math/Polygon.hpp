#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "haylen/math/Vec2.hpp"

namespace haylen::math {

// Boolean operations, offsets, simplification and convex decomposition of shapes. A shape is a list of closed outlines where any single outline is filled in either winding and an outline wound opposite to the one around it is a hole. Results wind outer outlines with a positive `Geometry::signedArea` and holes with a negative one, and keep three decimal places.
class Polygon final {
  public:
    using Outline = std::vector<Vec2>;

    enum class Join : std::uint8_t {
        Miter,
        Round,
        Square,
        Bevel,
    };

    [[nodiscard]] static std::vector<Outline> unite(std::span<const Outline> subjects, std::span<const Outline> clips = {});
    [[nodiscard]] static std::vector<Outline> subtract(std::span<const Outline> subjects, std::span<const Outline> clips);
    [[nodiscard]] static std::vector<Outline> intersect(std::span<const Outline> subjects, std::span<const Outline> clips);
    [[nodiscard]] static std::vector<Outline> exclude(std::span<const Outline> subjects, std::span<const Outline> clips);

    // Grows the shape by `distance`, or shrinks it when `distance` is negative. Round joins follow the arc within one percent of the distance.
    [[nodiscard]] static std::vector<Outline> offset(std::span<const Outline> shape, float distance, Join join = Join::Round, float miterLimit = 2.0F);

    // Drops the points closer than `tolerance` to the simplified line with Ramer-Douglas-Peucker. Open lines keep both ends, and closed outlines keep at least three points.
    [[nodiscard]] static Outline simplify(std::span<const Vec2> points, float tolerance, bool closed = true);

    // Splits a shape into convex pieces of at most `maxVertices` points, each wound with a positive signed area, by merging the triangles of a constrained Delaunay triangulation. Throws `std::invalid_argument` when `maxVertices` is below 3 or the triangulation fails.
    [[nodiscard]] static std::vector<Outline> decompose(std::span<const Outline> shape, std::size_t maxVertices = 8);

    // Returns the filled area of a shape, where holes subtract.
    [[nodiscard]] static float getArea(std::span<const Outline> shape);

  private:
    class Clipper;
    class Merger;

    static constexpr int kPrecision = 3;
    static constexpr double kScale = 1000.0;
    static constexpr float kArcTolerance = 0.01F;

    enum class Operation : std::uint8_t {
        Union,
        Difference,
        Intersection,
        Xor,
    };

    [[nodiscard]] static std::vector<Outline> combine(Operation operation, std::span<const Outline> subjects, std::span<const Outline> clips);
    [[nodiscard]] static float distanceSquaredToSegment(Vec2 point, Vec2 start, Vec2 end) noexcept;
    static void simplifyRange(std::span<const Vec2> points, std::size_t first, std::size_t last, float toleranceSquared, std::vector<bool>& kept);
};

} // namespace haylen::math
