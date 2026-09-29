#pragma once

#include <array>
#include <cstdint>

#include "haylen/2d/spatial/Cell.hpp"
#include "haylen/2d/spatial/CellGrid.hpp"

namespace haylen::tiled {
struct WangSet;
}

namespace haylen::procedural2d {

// Picks tile variants at run time from the neighbors of each cell: 4-bit masks of the sides, 8-bit masks reduced to the 47 tiles of a blob set, and the terrains of a Tiled Wang set.
class Autotile final {
  public:
    // Returns the sides whose neighbor holds the same value as the cell: north 1, east 2, south 4 and west 8. The cell must lie inside the grid, and cells beyond the grid match when edgesMatch is set.
    [[nodiscard]] static std::uint8_t getMask4(const spatial2d::CellGrid& terrain, spatial2d::Cell cell, bool edgesMatch = true) noexcept;

    // Returns the neighbors that hold the same value as the cell, clockwise from north: north 1, north-east 2, east 4, south-east 8, south 16, south-west 32, west 64 and north-west 128. A corner only counts when both sides next to it match, which leaves the 47 masks of a blob tile set.
    [[nodiscard]] static std::uint8_t getMask8(const spatial2d::CellGrid& terrain, spatial2d::Cell cell, bool edgesMatch = true) noexcept;

    // Numbers the 47 blob masks from 0 in increasing mask order, and returns -1 for masks that getMask8 never returns.
    [[nodiscard]] static int getBlobIndex(std::uint8_t mask) noexcept;

    // Returns the 4-bit mask of every cell that holds value, and -1 for the other cells.
    [[nodiscard]] static spatial2d::CellGrid apply4(const spatial2d::CellGrid& terrain, std::int32_t value, bool edgesMatch = true);
    // Returns the blob index of every cell that holds value, and -1 for the other cells.
    [[nodiscard]] static spatial2d::CellGrid apply8(const spatial2d::CellGrid& terrain, std::int32_t value, bool edgesMatch = true);

    // Returns the tileset tile of a Wang set for every cell, or -1 where no tile matches, from colors of at least 2 by 2 cells for corner and mixed sets. Colors are the Wang color numbers of Tiled, from 1, with 0 for no color.
    // Corner and mixed sets read colors at the corners of the cells from a grid one cell wider and taller than the result, where cell (x, y) has its corners at (x, y), (x + 1, y), (x + 1, y + 1) and (x, y + 1). Mixed sets color an edge when both of its corners agree and accept any color otherwise.
    // Edge sets read one color per cell and color each side whose neighbor shares that color, which suits paths and fences. Throws std::invalid_argument for an unknown kind of set.
    // When several tiles match, a hash of the cell and the seed picks one.
    [[nodiscard]] static spatial2d::CellGrid applyWang(const spatial2d::CellGrid& colors, const tiled::WangSet& set, std::uint64_t seed = 0);

  private:
    static constexpr std::uint8_t kAny = 255;

    // The neighbors clockwise from north, so the even entries are the sides.
    static constexpr std::array<spatial2d::Cell, 8> kOffsets{{{0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}}};

    [[nodiscard]] static constexpr std::uint8_t reduce(unsigned mask) noexcept {
        unsigned result = mask & 0x55U;
        if ((mask & 0x05U) == 0x05U) {
            result |= mask & 0x02U;
        }
        if ((mask & 0x14U) == 0x14U) {
            result |= mask & 0x08U;
        }
        if ((mask & 0x50U) == 0x50U) {
            result |= mask & 0x20U;
        }
        if ((mask & 0x41U) == 0x41U) {
            result |= mask & 0x80U;
        }
        return static_cast<std::uint8_t>(result);
    }

    [[nodiscard]] static constexpr std::array<std::int8_t, 256> makeBlobIndices() noexcept {
        std::array<std::int8_t, 256> indices{};
        std::int8_t next = 0;
        for (unsigned mask = 0; mask < 256; ++mask) {
            indices[mask] = reduce(mask) == mask ? next++ : std::int8_t{-1};
        }
        return indices;
    }

    static const std::array<std::int8_t, 256> kBlobIndices;

    [[nodiscard]] static bool matches(const spatial2d::CellGrid& terrain, spatial2d::Cell cell, std::int32_t value, bool edgesMatch) noexcept;
    [[nodiscard]] static std::uint8_t colorAt(const spatial2d::CellGrid& colors, spatial2d::Cell cell) noexcept;
    [[nodiscard]] static std::int32_t pickWangTile(const tiled::WangSet& set, const std::array<std::uint8_t, 8>& wanted, int x, int y, std::uint64_t seed) noexcept;
};

} // namespace haylen::procedural2d
