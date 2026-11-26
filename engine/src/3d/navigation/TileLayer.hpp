#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace haylen::navigation3d {

// One floor of one tile of a navigation mesh with obstacles: a grid of `width` by `height` voxels with the height, the area and the connections of each, compressed with Zstandard in that order, which an assembly marks with the obstacles that stand on it before it becomes polygons. Only the voxels from `minX` to `maxX` and from `minZ` to `maxZ` belong to the tile, and the rest is its border. Heights count voxels from `min`, and `minHeight` and `maxHeight` bound them.
struct TileLayer {
    int x = 0;
    int z = 0;
    int layer = 0;
    std::array<float, 3> min{};
    std::array<float, 3> max{};
    std::uint16_t minHeight = 0;
    std::uint16_t maxHeight = 0;
    std::uint8_t width = 0;
    std::uint8_t height = 0;
    std::uint8_t minX = 0;
    std::uint8_t maxX = 0;
    std::uint8_t minZ = 0;
    std::uint8_t maxZ = 0;
    std::vector<std::uint8_t> compressed;

    [[nodiscard]] std::size_t getCellCount() const noexcept {
        return static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    }

    friend bool operator==(const TileLayer&, const TileLayer&) = default;
};

} // namespace haylen::navigation3d
