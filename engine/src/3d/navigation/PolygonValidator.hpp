#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace haylen::navigation3d {

// Checks the polygons of one tile in the form that Detour builds its tiles from, whether a file stored them or the voxels of a layer made them, so a walk along the surface crosses each polygon once: convex polygons of one turn, indices within the tile, areas that the mesh names, neighbors that meet them back edge to edge, and portals on the sides of the tile.
class PolygonValidator final {
  public:
    // The polygons of a tile whose vertices count voxels, `maxVertices` corners padded with 0xFFFF and then one neighbor for each edge, on a tile of `width` by `depth` voxels.
    struct Polygons {
        std::span<const std::uint16_t> vertices;
        std::span<const std::uint16_t> polygons;
        std::span<const std::uint8_t> areas;
        int maxVertices = 0;
        std::size_t areaCount = 0;
        int width = 0;
        int depth = 0;
    };

    [[nodiscard]] static bool isValid(const Polygons& tile);

  private:
    [[nodiscard]] static std::size_t getUsed(const std::uint16_t* corners, std::size_t stride);

    // The way the corners of a polygon turn seen from above, 1 or -1, or 0 for a polygon that is not convex or has no area.
    [[nodiscard]] static int getTurn(const Polygons& tile, const std::uint16_t* corners, std::size_t used);

    // Whether the neighbor across the edge from `from` to `to` of a polygon names the polygon back across the same edge, and whether a portal edge lies on the side of the tile it names.
    [[nodiscard]] static bool meetsBack(const Polygons& tile, std::uint16_t neighbor, std::size_t polygon, std::uint16_t from, std::uint16_t to);
    [[nodiscard]] static bool isOnSide(const Polygons& tile, std::uint16_t from, std::uint16_t to, unsigned int side);
};

} // namespace haylen::navigation3d
