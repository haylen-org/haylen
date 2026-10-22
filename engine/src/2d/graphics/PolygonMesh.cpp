#include "2d/graphics/PolygonMesh.hpp"

#include <algorithm>
#include <bit>
#include <cmath>

#include "haylen/math/Geometry.hpp"

namespace haylen::graphics2d {

// The corner moves where the lines of its two edges meet once each line moves by its distance along its normal, which keeps a fading edge half a pixel from its line and lets a fill slide along an edge that does not fade. Edges on one line, or turning back on themselves, take the mean of their moves, and no corner moves further than the limit.
math::Vec2 PolygonMesh::getOffset(math::Vec2 before, math::Vec2 after, float beforeDistance, float afterDistance, float limit) noexcept {
    const float determinant = math::Vec2::cross(before, after);
    math::Vec2 offset = (before * beforeDistance + after * afterDistance) * 0.5F;
    if (std::fabs(determinant) > 1.0e-3F) {
        offset = math::Vec2{after.y * beforeDistance - before.y * afterDistance, before.x * afterDistance - after.x * beforeDistance} / determinant;
    }
    return offset.clampedLength(limit);
}

void PolygonMesh::build(std::span<const math::Vec2> points, std::span<const std::uint32_t> counts, std::uint32_t color, float pixel) {
    vertices.clear();
    indices.clear();
    triangulator.triangulate(points, counts);
    const std::vector<math::Triangulator::Kind>& kinds = triangulator.getKinds();

    // Every edge of an outer outline or a hole fades on the side away from the fill, unless it has no length or another outline shares it.
    normals.assign(points.size(), math::Vec2{});
    fades.assign(points.size(), 0);
    std::uint32_t first = 0;
    for (std::size_t outline = 0; outline < counts.size(); first += counts[outline++]) {
        const std::uint32_t count = counts[outline];
        if (kinds[outline] == math::Triangulator::Kind::Inside) {
            continue;
        }
        const float area = math::Geometry::signedArea(points.subspan(first, count));
        const float side = (area > 0.0F ? 1.0F : -1.0F) * (kinds[outline] == math::Triangulator::Kind::Hole ? -1.0F : 1.0F);
        for (std::uint32_t step = 0; step < count; ++step) {
            const math::Vec2 delta = points[first + (step + 1) % count] - points[first + step];
            const float length = delta.getLength();
            if (length > 0.0F) {
                normals[first + step] = math::Vec2{delta.y, -delta.x} * (side / length);
                fades[first + step] = 1;
            }
        }
    }
    findSharedEdges(points, counts);

    // Every point gives an inner vertex in the color, where the fill stops, and an outer one without alpha, where the fringe ends.
    const float half = pixel * 0.5F;
    const std::uint32_t clear = color & 0x00FFFFFFU;
    first = 0;
    for (const std::uint32_t count : counts) {
        for (std::uint32_t step = 0; step < count; ++step) {
            const std::uint32_t index = first + step;
            const std::uint32_t previous = first + (step + count - 1) % count;
            const math::Vec2 offset = getOffset(normals[previous], normals[index], fades[previous] != 0 ? half : 0.0F, fades[index] != 0 ? half : 0.0F, half * kMiterLimit);
            const math::Vec2 inner = points[index] - offset;
            const math::Vec2 outer = points[index] + offset;
            vertices.push_back({{inner.x, inner.y}, {0.5F, 0.5F}, color});
            vertices.push_back({{outer.x, outer.y}, {0.5F, 0.5F}, clear});
        }
        first += count;
    }

    for (const std::uint32_t index : triangulator.getTriangles()) {
        indices.push_back(index * 2U);
    }
    first = 0;
    for (const std::uint32_t count : counts) {
        for (std::uint32_t step = 0; step < count; ++step) {
            if (fades[first + step] == 0) {
                continue;
            }
            const std::uint32_t inner = (first + step) * 2U;
            const std::uint32_t next = (first + (step + 1) % count) * 2U;
            indices.insert(indices.end(), {inner, next, next + 1U, inner, next + 1U, inner + 1U});
        }
        first += count;
    }
}

// The edges that fade sort by the bits of their ends, so an edge that two outlines share, whichever way each runs along it, lands next to its copy and loses its fringe on both.
void PolygonMesh::findSharedEdges(std::span<const math::Vec2> points, std::span<const std::uint32_t> counts) {
    edges.clear();
    std::uint32_t first = 0;
    for (const std::uint32_t count : counts) {
        for (std::uint32_t step = 0; step < count; ++step) {
            if (fades[first + step] == 0) {
                continue;
            }
            const math::Vec2 from = points[first + step];
            const math::Vec2 to = points[first + (step + 1) % count];
            std::array<std::uint32_t, 4> key{std::bit_cast<std::uint32_t>(from.x), std::bit_cast<std::uint32_t>(from.y), std::bit_cast<std::uint32_t>(to.x), std::bit_cast<std::uint32_t>(to.y)};
            if (std::pair(key[2], key[3]) < std::pair(key[0], key[1])) {
                key = {key[2], key[3], key[0], key[1]};
            }
            edges.push_back({.key = key, .point = first + step});
        }
        first += count;
    }

    std::sort(edges.begin(), edges.end(), [](const Edge& lhs, const Edge& rhs) { return lhs.key < rhs.key; });
    for (std::size_t start = 0; start < edges.size();) {
        std::size_t end = start + 1;
        while (end < edges.size() && edges[end].key == edges[start].key) {
            ++end;
        }
        if (end - start > 1) {
            for (std::size_t copy = start; copy < end; ++copy) {
                fades[edges[copy].point] = 0;
            }
        }
        start = end;
    }
}

} // namespace haylen::graphics2d
