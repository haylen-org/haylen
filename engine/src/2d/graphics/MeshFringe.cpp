#include "2d/graphics/MeshFringe.hpp"

#include <algorithm>
#include <bit>
#include <cmath>

#include "2d/graphics/PolygonMesh.hpp"

namespace haylen::graphics2d {

std::array<std::uint32_t, 2> MeshFringe::keyOf(const GpuVertex& vertex) noexcept {
    return {std::bit_cast<std::uint32_t>(vertex.position[0]), std::bit_cast<std::uint32_t>(vertex.position[1])};
}

const MeshFringe::Corner* MeshFringe::findCorner(const GpuVertex& vertex) const noexcept {
    const std::array<std::uint32_t, 2> key = keyOf(vertex);
    const auto found = std::ranges::lower_bound(corners, key, {}, &Corner::key);
    return found != corners.end() && found->key == key ? &*found : nullptr;
}

void MeshFringe::build(std::span<const GpuVertex> meshVertices, std::span<const std::uint32_t> meshIndices, float pixel) {
    vertices.assign(meshVertices.begin(), meshVertices.end());
    indices.assign(meshIndices.begin(), meshIndices.end());

    // Every edge of a triangle with an area, with its normal turned away from the third vertex of the triangle.
    edges.clear();
    for (std::size_t first = 0; first + 2 < meshIndices.size(); first += 3) {
        const std::array<std::uint32_t, 3> points{meshIndices[first], meshIndices[first + 1], meshIndices[first + 2]};
        const std::array<math::Vec2, 3> places{math::Vec2{meshVertices[points[0]].position[0], meshVertices[points[0]].position[1]}, math::Vec2{meshVertices[points[1]].position[0], meshVertices[points[1]].position[1]}, math::Vec2{meshVertices[points[2]].position[0], meshVertices[points[2]].position[1]}};
        if (math::Vec2::cross(places[1] - places[0], places[2] - places[0]) == 0.0F) {
            continue;
        }
        for (std::size_t side = 0; side < 3; ++side) {
            const std::size_t next = (side + 1) % 3;
            const math::Vec2 delta = places[next] - places[side];
            math::Vec2 normal = math::Vec2{delta.y, -delta.x}.getNormalized();
            if (math::Vec2::dot(normal, places[(side + 2) % 3] - places[side]) > 0.0F) {
                normal = -normal;
            }
            const std::array<std::uint32_t, 2> start = keyOf(meshVertices[points[side]]);
            const std::array<std::uint32_t, 2> end = keyOf(meshVertices[points[next]]);
            const std::array<std::uint32_t, 4> key = start < end ? std::array{start[0], start[1], end[0], end[1]} : std::array{end[0], end[1], start[0], start[1]};
            edges.push_back({.key = key, .from = points[side], .to = points[next], .normal = normal});
        }
    }

    // The edges that no other triangle shares are the outline.
    std::ranges::sort(edges, {}, &Edge::key);
    outline.clear();
    for (std::size_t start = 0; start < edges.size();) {
        std::size_t end = start + 1;
        while (end < edges.size() && edges[end].key == edges[start].key) {
            ++end;
        }
        if (end - start == 1) {
            outline.push_back(edges[start]);
        }
        start = end;
    }

    // Each place on the outline moves where the lines of the edges that meet there meet once each moves half a pixel along its normal.
    const float half = pixel * 0.5F;
    corners.clear();
    for (const Edge& edge : outline) {
        corners.push_back({.key = keyOf(vertices[edge.from]), .normal = edge.normal});
        corners.push_back({.key = keyOf(vertices[edge.to]), .normal = edge.normal});
    }
    std::ranges::sort(corners, {}, &Corner::key);
    std::size_t kept = 0;
    for (std::size_t start = 0; start < corners.size();) {
        std::size_t end = start + 1;
        math::Vec2 sum = corners[start].normal;
        while (end < corners.size() && corners[end].key == corners[start].key) {
            sum += corners[end].normal;
            ++end;
        }
        const math::Vec2 offset = end - start == 2 ? PolygonMesh::getOffset(corners[start].normal, corners[start + 1].normal, half, half, half * kMiterLimit) : sum.getNormalized() * half;
        corners[kept++] = {.key = corners[start].key, .offset = offset};
        start = end;
    }
    corners.resize(kept);

    // The outline moves half a pixel in, and every edge of it gains a fringe whose outer side fades to nothing half a pixel out.
    for (GpuVertex& vertex : vertices) {
        if (const Corner* corner = findCorner(vertex); corner != nullptr) {
            vertex.position[0] -= corner->offset.x;
            vertex.position[1] -= corner->offset.y;
        }
    }
    for (const Edge& edge : outline) {
        const auto base = static_cast<std::uint32_t>(vertices.size());
        for (const std::uint32_t point : {edge.from, edge.to}) {
            GpuVertex outer = vertices[point];
            const Corner& corner = *findCorner(meshVertices[point]);
            outer.position[0] = meshVertices[point].position[0] + corner.offset.x;
            outer.position[1] = meshVertices[point].position[1] + corner.offset.y;
            outer.color &= 0x00FFFFFFU;
            vertices.push_back(outer);
        }
        indices.insert(indices.end(), {edge.from, edge.to, base + 1U, edge.from, base + 1U, base});
    }
}

} // namespace haylen::graphics2d
