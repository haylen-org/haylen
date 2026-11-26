#pragma once

#include <DetourNavMesh.h>
#include <DetourNavMeshQuery.h>

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

#include "haylen/math/Random.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::navigation3d {

// Picks points of a navigation mesh where every square meter of ground, seen from above, is as likely as any other: across the whole mesh, or inside a circle around a point that the point reaches, whose polygons are cut by the circle before they are weighed.
class RandomPoints final {
  public:
    struct Point {
        std::array<float, 3> position{};
        dtPolyRef polygon = 0;
    };

    // The buffers that a pick reuses, which grow to the largest pick and then stay.
    struct Buffers {
        std::vector<dtPolyRef> polygons;
        std::vector<dtPolyRef> parents;
        std::vector<float> areas;
    };

    static constexpr std::size_t kCircleSides = 32;
    static constexpr std::size_t kMaxAround = 512;

    [[nodiscard]] static std::optional<Point> onMesh(const dtNavMesh& mesh, const dtNavMeshQuery& query, const dtQueryFilter& filter, math::Random& random);
    [[nodiscard]] static std::optional<Point> around(const dtNavMesh& mesh, const dtNavMeshQuery& query, const dtQueryFilter& filter, dtPolyRef start, const std::array<float, 3>& center, float radius, math::Random& random, Buffers& buffers);

  private:
    // A convex outline on the ground, from the corners of a polygon cut by the sides of a circle.
    struct Outline {
        std::array<math::Vec2, 6 + kCircleSides> points{};
        std::size_t count = 0;
    };

    [[nodiscard]] static Outline getOutline(const dtMeshTile& tile, const dtPoly& poly);
    [[nodiscard]] static Outline clip(const Outline& outline, math::Vec2 center, float radius);
    [[nodiscard]] static float getArea(const Outline& outline);
    [[nodiscard]] static math::Vec2 pickInside(const Outline& outline, math::Random& random);
    [[nodiscard]] static std::optional<Point> place(const dtNavMeshQuery& query, dtPolyRef polygon, math::Vec2 ground, float height);
};

} // namespace haylen::navigation3d
