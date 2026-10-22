#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "2d/graphics/GpuVertex.hpp"
#include "haylen/math/Vec2.hpp"
#include "math/Triangulator.hpp"

namespace haylen::graphics2d {

// Builds the triangles of a filled shape whose edge fades out over one pixel: the fill stops half a pixel inside the outline and a fringe fades it to nothing half a pixel outside, so a pixel the outline crosses takes the share of it the shape covers. The outlines fill by the nonzero rule of `math::Triangulator`, so outlines wound against the outline around them cut holes, and an edge that two outlines share, such as the edge between two convex pieces of a shape or between two chunks of a terrain, joins their fills without a fringe, so only the outline of the whole shape and of its holes fades. The storage stays from shape to shape, so a shape drawn every frame allocates nothing once it has grown.
class PolygonMesh final {
  public:
    // Replaces the vertices and indices with the shape whose outlines are the runs of `counts` points that follow each other in `points`, in the color, where `pixel` is the size of one pixel in the units of the points.
    void build(std::span<const math::Vec2> points, std::span<const std::uint32_t> counts, std::uint32_t color, float pixel);

    [[nodiscard]] const std::vector<GpuVertex>& getVertices() const noexcept {
        return vertices;
    }
    [[nodiscard]] const std::vector<std::uint32_t>& getIndices() const noexcept {
        return indices;
    }

  private:
    // A sharp corner moves its fill and its fringe at most this many half pixels from its point.
    static constexpr float kMiterLimit = 2.0F;

    // An edge from a point of an outline to the next one, keyed by the bits of its ends in a fixed order, so the copies of an edge that two outlines share, whichever way they run, sort next to each other.
    struct Edge {
        std::array<std::uint32_t, 4> key{};
        std::uint32_t point = 0;
    };

    // The offset of a corner whose edges before and after it, with the given normals that point away from the fill, move by the given distances along them.
    [[nodiscard]] static math::Vec2 getOffset(math::Vec2 before, math::Vec2 after, float beforeDistance, float afterDistance, float limit) noexcept;

    void findSharedEdges(std::span<const math::Vec2> points, std::span<const std::uint32_t> counts);

    math::Triangulator triangulator;
    std::vector<Edge> edges;

    // For every point, the normal of the edge that starts at it, pointing away from the fill, and whether that edge fades.
    std::vector<math::Vec2> normals;
    std::vector<std::uint8_t> fades;
    std::vector<GpuVertex> vertices;
    std::vector<std::uint32_t> indices;
};

} // namespace haylen::graphics2d
