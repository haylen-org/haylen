#include "haylen/2d/procedural/Autotile.hpp"

#include <algorithm>
#include <stdexcept>

#include "haylen/2d/tiled/WangSet.hpp"

namespace haylen::procedural2d {

constexpr std::array<std::int8_t, 256> Autotile::kBlobIndices = makeBlobIndices();

bool Autotile::matches(const spatial2d::CellGrid& terrain, spatial2d::Cell cell, std::int32_t value, bool edgesMatch) noexcept {
    return terrain.contains(cell) ? terrain[cell] == value : edgesMatch;
}

std::uint8_t Autotile::getMask4(const spatial2d::CellGrid& terrain, spatial2d::Cell cell, bool edgesMatch) noexcept {
    const std::int32_t value = terrain[cell];
    unsigned mask = 0;
    for (std::size_t side = 0; side < 4; ++side) {
        const spatial2d::Cell offset = kOffsets[side * 2];
        if (matches(terrain, {cell.x + offset.x, cell.y + offset.y}, value, edgesMatch)) {
            mask |= 1U << side;
        }
    }
    return static_cast<std::uint8_t>(mask);
}

std::uint8_t Autotile::getMask8(const spatial2d::CellGrid& terrain, spatial2d::Cell cell, bool edgesMatch) noexcept {
    const std::int32_t value = terrain[cell];
    unsigned mask = 0;
    for (std::size_t bit = 0; bit < kOffsets.size(); ++bit) {
        if (matches(terrain, {cell.x + kOffsets[bit].x, cell.y + kOffsets[bit].y}, value, edgesMatch)) {
            mask |= 1U << bit;
        }
    }
    return reduce(mask);
}

int Autotile::getBlobIndex(std::uint8_t mask) noexcept {
    return kBlobIndices[mask];
}

spatial2d::CellGrid Autotile::apply4(const spatial2d::CellGrid& terrain, std::int32_t value, bool edgesMatch) {
    spatial2d::CellGrid result(terrain.getWidth(), terrain.getHeight(), -1);
    for (int y = 0; y < terrain.getHeight(); ++y) {
        for (int x = 0; x < terrain.getWidth(); ++x) {
            if (terrain[{x, y}] == value) {
                result.set({x, y}, getMask4(terrain, {x, y}, edgesMatch));
            }
        }
    }
    return result;
}

spatial2d::CellGrid Autotile::apply8(const spatial2d::CellGrid& terrain, std::int32_t value, bool edgesMatch) {
    spatial2d::CellGrid result(terrain.getWidth(), terrain.getHeight(), -1);
    for (int y = 0; y < terrain.getHeight(); ++y) {
        for (int x = 0; x < terrain.getWidth(); ++x) {
            if (terrain[{x, y}] == value) {
                result.set({x, y}, getBlobIndex(getMask8(terrain, {x, y}, edgesMatch)));
            }
        }
    }
    return result;
}

std::uint8_t Autotile::colorAt(const spatial2d::CellGrid& colors, spatial2d::Cell cell) noexcept {
    return colors.contains(cell) ? static_cast<std::uint8_t>(std::clamp(colors[cell], 0, 254)) : std::uint8_t{0};
}

// Chooses among the tiles whose Wang id matches every wanted position, with the same choice for the same cell and seed.
std::int32_t Autotile::pickWangTile(const tiled::WangSet& set, const std::array<std::uint8_t, 8>& wanted, int x, int y, std::uint64_t seed) noexcept {
    std::uint32_t matchCount = 0;
    std::int32_t chosen = -1;
    std::uint64_t hash = seed ^ (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) * 0x9E3779B97F4A7C15ULL) ^ (static_cast<std::uint64_t>(static_cast<std::uint32_t>(y)) * 0xC2B2AE3D27D4EB4FULL);
    for (const tiled::WangSet::Tile& tile : set.tiles) {
        bool matching = true;
        for (std::size_t position = 0; position < wanted.size() && matching; ++position) {
            matching = wanted[position] == kAny || wanted[position] == tile.wangId[position];
        }
        if (!matching) {
            continue;
        }

        // Reservoir sampling keeps one match with equal chances without storing them all.
        ++matchCount;
        hash = (hash ^ (hash >> 31U)) * 0xBF58476D1CE4E5B9ULL;
        if (hash % matchCount == 0) {
            chosen = static_cast<std::int32_t>(tile.tileId);
        }
    }
    return chosen;
}

spatial2d::CellGrid Autotile::applyWang(const spatial2d::CellGrid& colors, const tiled::WangSet& set, std::uint64_t seed) {
    const bool corners = set.kind == "corner" || set.kind == "mixed";
    if (!corners && set.kind != "edge") {
        throw std::invalid_argument("Wang sets must be of the corner, edge or mixed kind.");
    }

    const int width = corners ? colors.getWidth() - 1 : colors.getWidth();
    const int height = corners ? colors.getHeight() - 1 : colors.getHeight();
    spatial2d::CellGrid result(width, height, -1);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Tiled orders Wang ids as top, top right, right, bottom right, bottom, bottom left, left and top left.
            std::array<std::uint8_t, 8> wanted{};
            if (corners) {
                wanted[1] = colorAt(colors, {x + 1, y});
                wanted[3] = colorAt(colors, {x + 1, y + 1});
                wanted[5] = colorAt(colors, {x, y + 1});
                wanted[7] = colorAt(colors, {x, y});
                if (set.kind == "mixed") {
                    wanted[0] = wanted[7] == wanted[1] ? wanted[1] : kAny;
                    wanted[2] = wanted[1] == wanted[3] ? wanted[3] : kAny;
                    wanted[4] = wanted[3] == wanted[5] ? wanted[5] : kAny;
                    wanted[6] = wanted[5] == wanted[7] ? wanted[7] : kAny;
                }
            } else {
                const std::uint8_t color = colorAt(colors, {x, y});
                if (color == 0) {
                    continue;
                }
                wanted[0] = colorAt(colors, {x, y - 1}) == color ? color : std::uint8_t{0};
                wanted[2] = colorAt(colors, {x + 1, y}) == color ? color : std::uint8_t{0};
                wanted[4] = colorAt(colors, {x, y + 1}) == color ? color : std::uint8_t{0};
                wanted[6] = colorAt(colors, {x - 1, y}) == color ? color : std::uint8_t{0};
            }
            result.set({x, y}, pickWangTile(set, wanted, x, y, seed));
        }
    }
    return result;
}

} // namespace haylen::procedural2d
