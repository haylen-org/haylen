#include "2d/graphics/PolygonMesh.hpp"

#include <algorithm>
#include <cmath>

namespace haylen::graphics2d {

// The normal of an edge that points away from the inside of a polygon of the winding, or nothing for an edge without length.
math::Vec2 PolygonMesh::getOutward(math::Vec2 from, math::Vec2 to, float winding) noexcept {
    const math::Vec2 delta = to - from;
    const float length = delta.getLength();
    if (length <= 0.0F) {
        return {};
    }
    return math::Vec2{delta.y, -delta.x} * (winding / length);
}

void PolygonMesh::build(std::span<const math::Vec2> points, std::span<const std::uint32_t> triangles, std::uint32_t color, float pixel, std::vector<GpuVertex>& vertices, std::vector<std::uint32_t>& indices) {
    vertices.clear();
    indices.clear();
    const std::size_t count = points.size();
    if (count < 3 || triangles.empty()) {
        return;
    }

    float area = 0.0F;
    for (std::size_t index = 0; index < count; ++index) {
        const math::Vec2 current = points[index];
        const math::Vec2 next = points[(index + 1) % count];
        area += current.x * next.y - next.x * current.y;
    }
    const float winding = area >= 0.0F ? 1.0F : -1.0F;

    // Every point moves half a pixel along the bisector of its edges, in for the fill and out for the end of the fringe, and a sharp corner moves no further than the miter limit.
    const float half = pixel * 0.5F;
    const std::uint32_t clear = color & 0x00FFFFFFU;
    for (std::size_t index = 0; index < count; ++index) {
        const math::Vec2 point = points[index];
        const math::Vec2 before = getOutward(points[(index + count - 1) % count], point, winding);
        const math::Vec2 after = getOutward(point, points[(index + 1) % count], winding);
        const math::Vec2 middle = (before + after) * 0.5F;
        const float length = middle.getLength();
        const math::Vec2 offset = length > 0.0F ? middle * (std::min(1.0F / length, kMiterLimit) * half / length) : math::Vec2{};
        const math::Vec2 inner = point - offset;
        const math::Vec2 outer = point + offset;
        vertices.push_back({{inner.x, inner.y}, {0.5F, 0.5F}, color});
        vertices.push_back({{outer.x, outer.y}, {0.5F, 0.5F}, clear});
    }

    for (const std::uint32_t index : triangles) {
        indices.push_back(index * 2U);
    }
    for (std::size_t index = 0; index < count; ++index) {
        const auto inner = static_cast<std::uint32_t>(index * 2);
        const auto next = static_cast<std::uint32_t>(((index + 1) % count) * 2);
        indices.insert(indices.end(), {inner, next, next + 1U, inner, next + 1U, inner + 1U});
    }
}

} // namespace haylen::graphics2d
