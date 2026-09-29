#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "haylen/2d/spatial/CellGrid.hpp"

namespace haylen::math {
class Random;
}

namespace haylen::procedural2d {

// Fills a grid with tiles so that every pair of neighbors follows adjacency rules, the simple tiled model of Wave Function Collapse. It settles the cell with the fewest choices left, picks one of its tiles by weight and spreads what that rules out, restarting when a cell runs out of tiles.
class WaveFunctionCollapse final {
  public:
    enum class Direction : std::uint8_t {
        Right,
        Down,
        Left,
        Up,
    };

    // Which tiles may sit next to which, and how often each tile appears. Tiles are numbered from 0.
    class Rules final {
      public:
        // Starts with no allowed neighbors and a weight of 1 for every tile. Throws std::invalid_argument without tiles.
        explicit Rules(std::size_t tiles);

        // Learns the neighbors and the tile frequencies of a sample whose cells hold tile numbers. Periodic samples also pair the cells across opposite edges. Throws std::invalid_argument for negative cells.
        [[nodiscard]] static Rules fromSample(const spatial2d::CellGrid& sample, bool periodic = false);

        // Lets second sit on the given side of first, which also lets first sit on the opposite side of second. Throws std::out_of_range for unknown tiles.
        void allow(std::size_t first, std::size_t second, Direction direction);
        // Tiles with a weight of 0 never appear. Throws std::out_of_range for unknown tiles and std::invalid_argument for negative weights.
        void setWeight(std::size_t tile, float weight);

        [[nodiscard]] bool isAllowed(std::size_t first, std::size_t second, Direction direction) const noexcept;
        [[nodiscard]] float getWeight(std::size_t tile) const noexcept;
        [[nodiscard]] std::size_t getTileCount() const noexcept {
            return tileCount;
        }

      private:
        void requireTile(std::size_t tile) const;

        std::size_t tileCount;
        std::vector<float> weights;
        std::array<std::vector<bool>, 4> allowed;
    };

    // Fixed cells hold the tile a cell must take, or -1 to leave it free, and fixed must match the size when present. Periodic outputs also match the tiles across opposite edges.
    struct Options {
        int width = 32;
        int height = 32;
        bool periodic = false;
        int attempts = 10;
        std::optional<spatial2d::CellGrid> fixed;
    };

    // Returns the tile of every cell, or nothing when every attempt ran into a contradiction. Throws std::invalid_argument when the fixed grid does not match the size.
    [[nodiscard]] static std::optional<spatial2d::CellGrid> generate(const Rules& rules, const Options& options, math::Random& random);

    // Tells whether every pair of neighbors in the grid follows the rules. Cells that hold no tile of the rules break them.
    [[nodiscard]] static bool isValid(const spatial2d::CellGrid& tiles, const Rules& rules, bool periodic = false) noexcept;

  private:
    class Solver;

    [[nodiscard]] static Direction opposite(Direction direction) noexcept;
};

} // namespace haylen::procedural2d
