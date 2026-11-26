#include "3d/navigation/PolygonValidator.hpp"

#include <algorithm>

namespace haylen::navigation3d {

std::size_t PolygonValidator::getUsed(const std::uint16_t* corners, std::size_t stride) {
    return static_cast<std::size_t>(std::find(corners, corners + stride, std::uint16_t{0xFFFF}) - corners);
}

bool PolygonValidator::isValid(const Polygons& tile) {
    const auto stride = static_cast<std::size_t>(tile.maxVertices);
    const std::size_t vertexCount = tile.vertices.size() / 3;
    const std::size_t polygonCount = tile.areas.size();
    if (stride < 3 || tile.polygons.size() != polygonCount * stride * 2) {
        return false;
    }

    for (std::size_t polygon = 0; polygon < polygonCount; ++polygon) {
        const std::uint16_t* corners = &tile.polygons[polygon * stride * 2];
        const std::uint16_t* neighbors = corners + stride;
        const std::size_t used = getUsed(corners, stride);
        if (used < 3 || tile.areas[polygon] > tile.areaCount) {
            return false;
        }

        for (std::size_t corner = 0; corner < stride; ++corner) {
            const bool inside = corner < used;
            if (inside && corners[corner] >= vertexCount) {
                return false;
            }
            if (!inside && corners[corner] != 0xFFFF) {
                return false;
            }

            // A neighbor is a polygon of the tile, a wall, or a side of the tile marked by its top bit.
            const std::uint16_t neighbor = neighbors[corner];
            if (inside && neighbor != 0xFFFF && (neighbor & 0x8000U) == 0 && neighbor >= polygonCount) {
                return false;
            }
        }
    }

    int turn = 0;
    for (std::size_t polygon = 0; polygon < polygonCount; ++polygon) {
        const std::uint16_t* corners = &tile.polygons[polygon * stride * 2];
        const std::uint16_t* neighbors = corners + stride;
        const std::size_t used = getUsed(corners, stride);
        const int polygonTurn = getTurn(tile, corners, used);
        if (polygonTurn == 0 || (turn != 0 && polygonTurn != turn)) {
            return false;
        }
        turn = polygonTurn;

        for (std::size_t corner = 0; corner < used; ++corner) {
            const std::uint16_t from = corners[corner];
            const std::uint16_t to = corners[(corner + 1) % used];
            const std::uint16_t neighbor = neighbors[corner];
            const bool joined = neighbor == 0xFFFF || ((neighbor & 0x8000U) != 0 ? isOnSide(tile, from, to, neighbor & 0xFU) : meetsBack(tile, neighbor, polygon, from, to));
            if (!joined) {
                return false;
            }
        }
    }

    return true;
}

int PolygonValidator::getTurn(const Polygons& tile, const std::uint16_t* corners, std::size_t used) {
    // Every corner turns the same way as the whole polygon, whose area is not zero, in the exact numbers of the voxels.
    std::int64_t area = 0;
    int turn = 0;
    for (std::size_t corner = 0; corner < used; ++corner) {
        const std::uint16_t* a = &tile.vertices[static_cast<std::size_t>(corners[corner]) * 3];
        const std::uint16_t* b = &tile.vertices[static_cast<std::size_t>(corners[(corner + 1) % used]) * 3];
        const std::uint16_t* c = &tile.vertices[static_cast<std::size_t>(corners[(corner + 2) % used]) * 3];
        area += static_cast<std::int64_t>(a[0]) * b[2] - static_cast<std::int64_t>(b[0]) * a[2];
        const std::int64_t cross = (static_cast<std::int64_t>(b[0]) - a[0]) * (static_cast<std::int64_t>(c[2]) - b[2]) - (static_cast<std::int64_t>(b[2]) - a[2]) * (static_cast<std::int64_t>(c[0]) - b[0]);
        const int cornerTurn = cross > 0 ? 1 : (cross < 0 ? -1 : 0);
        if (cornerTurn != 0 && turn != 0 && cornerTurn != turn) {
            return 0;
        }
        turn = cornerTurn != 0 ? cornerTurn : turn;
    }

    const int areaTurn = area > 0 ? 1 : (area < 0 ? -1 : 0);
    return areaTurn == turn ? turn : 0;
}

bool PolygonValidator::meetsBack(const Polygons& tile, std::uint16_t neighbor, std::size_t polygon, std::uint16_t from, std::uint16_t to) {
    if (neighbor == polygon) {
        return false;
    }

    const auto stride = static_cast<std::size_t>(tile.maxVertices);
    const std::uint16_t* corners = &tile.polygons[static_cast<std::size_t>(neighbor) * stride * 2];
    const std::uint16_t* neighbors = corners + stride;
    const std::size_t used = getUsed(corners, stride);
    for (std::size_t corner = 0; corner < used; ++corner) {
        if (corners[corner] == to && corners[(corner + 1) % used] == from && neighbors[corner] == polygon) {
            return true;
        }
    }

    return false;
}

bool PolygonValidator::isOnSide(const Polygons& tile, std::uint16_t from, std::uint16_t to, unsigned int side) {
    // The sides count from smaller x, larger z, larger x and smaller z, and 15 marks a wall on the border.
    const std::uint16_t* a = &tile.vertices[static_cast<std::size_t>(from) * 3];
    const std::uint16_t* b = &tile.vertices[static_cast<std::size_t>(to) * 3];
    const auto width = static_cast<std::uint16_t>(tile.width);
    const auto depth = static_cast<std::uint16_t>(tile.depth);
    switch (side) {
    case 0:
        return a[0] == 0 && b[0] == 0;
    case 1:
        return a[2] == depth && b[2] == depth;
    case 2:
        return a[0] == width && b[0] == width;
    case 3:
        return a[2] == 0 && b[2] == 0;
    case 0xF:
        return true;
    default:
        return false;
    }
}

} // namespace haylen::navigation3d
