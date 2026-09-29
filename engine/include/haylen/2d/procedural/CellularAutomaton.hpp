#pragma once

#include <cstdint>

#include "haylen/2d/spatial/Cell.hpp"
#include "haylen/2d/spatial/CellGrid.hpp"

namespace haylen::math {
class Random;
}

namespace haylen::procedural2d {

// Grows cave maps with a cellular automaton, where 1 marks walls and 0 floors: random walls smoothed by counting the walls around each cell.
class CellularAutomaton final {
  public:
    static constexpr std::int32_t kFloor = 0;
    static constexpr std::int32_t kWall = 1;

    // A floor cell turns into a wall with at least birthLimit walls among its eight neighbors, and a wall stays a wall with at least survivalLimit. A solid border keeps the outermost cells as walls and counts cells beyond the grid as walls.
    struct Options {
        int width = 64;
        int height = 64;
        float fillChance = 0.45F;
        int steps = 5;
        int birthLimit = 5;
        int survivalLimit = 4;
        bool solidBorder = true;
    };

    [[nodiscard]] static spatial2d::CellGrid generate(const Options& options, math::Random& random);

    // Runs one smoothing step over any grid, which only reads the birth and survival limits and the solid border of the options.
    [[nodiscard]] static spatial2d::CellGrid step(const spatial2d::CellGrid& grid, const Options& options);

  private:
    [[nodiscard]] static bool isBorder(spatial2d::Cell cell, int width, int height) noexcept;
    [[nodiscard]] static int countWalls(const spatial2d::CellGrid& grid, spatial2d::Cell cell, bool solidBorder) noexcept;
};

} // namespace haylen::procedural2d
