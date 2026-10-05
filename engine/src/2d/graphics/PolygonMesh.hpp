#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "2d/graphics/GpuVertex.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics2d {

// Builds the triangles of a filled polygon whose edge fades out over one pixel: the fill stops half a pixel inside the outline, and a fringe fades from it to nothing half a pixel outside, so a pixel the outline crosses takes the share of it the polygon covers.
class PolygonMesh final {
  public:
    // Replaces the vertices and indices with the polygon of the points in the color, filled by the triangles that index the points, where `pixel` is the size of one pixel in the units of the points.
    static void build(std::span<const math::Vec2> points, std::span<const std::uint32_t> triangles, std::uint32_t color, float pixel, std::vector<GpuVertex>& vertices, std::vector<std::uint32_t>& indices);

  private:
    // A sharp corner moves its fringe at most this many half pixels along its bisector.
    static constexpr float kMiterLimit = 2.0F;

    [[nodiscard]] static math::Vec2 getOutward(math::Vec2 from, math::Vec2 to, float winding) noexcept;
};

} // namespace haylen::graphics2d
