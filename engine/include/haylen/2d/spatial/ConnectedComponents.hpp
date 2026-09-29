#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "haylen/2d/spatial/CellGrid.hpp"

namespace haylen::spatial2d {

// Splits a grid into regions of connected cells that share a value, such as the islands of a map or the rooms of a dungeon.
class ConnectedComponents final {
  public:
    struct Options {
        bool diagonal = false;
        std::optional<std::int32_t> background;
    };

    // Writes the region number of every cell into labels, which must have the size of the grid, and returns the number of regions. Regions are numbered from 1 in the order their first cell appears row by row, and cells with the background value get 0. Diagonal regions also join cells that only share a corner.
    static std::size_t label(const CellGrid& grid, const Options& options, CellGrid& labels);
};

} // namespace haylen::spatial2d
