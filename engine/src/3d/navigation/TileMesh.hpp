#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace haylen::navigation3d {

// The polygons of one tile of a navigation mesh without obstacles, with the detail triangles that follow the height of the ground inside them, in the form that Detour builds its tiles from. Vertices count voxels from `min`. Each polygon lists `maxVertices` vertices, padded with 0xFFFF, and then as many neighbors, one for each edge: the polygon across it, 0xFFFF for a wall, or the bit 0x8000 with a side from 0 to 3 for a portal to the next tile, or with 15 for a wall on the border of the tile. Each detail mesh is the first detail vertex, the detail vertex count, the first detail triangle and the triangle count of its polygon, and each detail triangle names three vertices of its polygon and then of its detail vertices, followed by the flags of its edges.
struct TileMesh {
    int x = 0;
    int z = 0;
    int layer = 0;
    std::array<float, 3> min{};
    std::array<float, 3> max{};
    int maxVertices = 6;
    std::vector<std::uint16_t> vertices;
    std::vector<std::uint16_t> polygons;
    std::vector<std::uint8_t> areas;
    std::vector<std::uint32_t> detailMeshes;
    std::vector<float> detailVertices;
    std::vector<std::uint8_t> detailTriangles;

    [[nodiscard]] std::size_t getPolygonCount() const noexcept {
        return areas.size();
    }
    [[nodiscard]] std::size_t getVertexCount() const noexcept {
        return vertices.size() / 3;
    }

    friend bool operator==(const TileMesh&, const TileMesh&) = default;
};

} // namespace haylen::navigation3d
