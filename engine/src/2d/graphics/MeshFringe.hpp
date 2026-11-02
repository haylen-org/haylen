#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "2d/graphics/GpuVertex.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics2d {

// Fades the outline of a mesh over one pixel, as the edges of every other shape fade. The outline is every edge that only one triangle holds, where vertices at the same place count as one, so the seams of a mesh whose triangles keep vertices of their own stay inside it. The outline moves half a pixel in, carrying the vertices on it, and a fringe fades from it to nothing half a pixel outside, in the colors and texture coordinates of its vertices. The storage stays from mesh to mesh, so a mesh drawn every frame allocates nothing once it has grown.
class MeshFringe final {
  public:
    // Replaces the vertices and indices with the mesh and the fringe of its outline, where `pixel` is the size of one pixel in the units of the vertices.
    void build(std::span<const GpuVertex> meshVertices, std::span<const std::uint32_t> meshIndices, float pixel);

    [[nodiscard]] const std::vector<GpuVertex>& getVertices() const noexcept {
        return vertices;
    }
    [[nodiscard]] const std::vector<std::uint32_t>& getIndices() const noexcept {
        return indices;
    }

  private:
    // An edge of a triangle, keyed by the bits of the places of its ends in a fixed order, so the copies of an edge that two triangles share sort next to each other, with its ends in the order of its triangle and the normal that points away from the triangle.
    struct Edge {
        std::array<std::uint32_t, 4> key{};
        std::uint32_t from = 0;
        std::uint32_t to = 0;
        math::Vec2 normal{};
    };

    // A place on the outline, keyed by the bits of its position, with the normal of an outline edge that meets there and how far the place moves.
    struct Corner {
        std::array<std::uint32_t, 2> key{};
        math::Vec2 normal{};
        math::Vec2 offset{};
    };

    // A sharp corner moves its fringe at most this many half pixels from its point.
    static constexpr float kMiterLimit = 2.0F;

    [[nodiscard]] static std::array<std::uint32_t, 2> keyOf(const GpuVertex& vertex) noexcept;
    [[nodiscard]] const Corner* findCorner(const GpuVertex& vertex) const noexcept;

    std::vector<Edge> edges;
    std::vector<Edge> outline;
    std::vector<Corner> corners;
    std::vector<GpuVertex> vertices;
    std::vector<std::uint32_t> indices;
};

} // namespace haylen::graphics2d
