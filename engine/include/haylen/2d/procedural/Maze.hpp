#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "haylen/2d/spatial/CellGrid.hpp"

namespace haylen::math {
class Random;
}

namespace haylen::procedural2d {

// A maze of cells where each cell records the sides it opens to. Generated mazes are perfect: exactly one path joins any two cells. The recursive backtracker makes long winding corridors, while Prim and Kruskal make many short dead ends.
class Maze final {
  public:
    enum class Algorithm : std::uint8_t {
        Backtracker,
        Prim,
        Kruskal,
    };

    static constexpr std::uint8_t kNorth = 1;
    static constexpr std::uint8_t kEast = 2;
    static constexpr std::uint8_t kSouth = 4;
    static constexpr std::uint8_t kWest = 8;

    // Starts with every wall closed. Throws std::invalid_argument when a side is below 1.
    Maze(int columns, int rows);

    [[nodiscard]] static Maze generate(int columns, int rows, Algorithm algorithm, math::Random& random);
    [[nodiscard]] static std::optional<Algorithm> algorithmFromName(std::string_view name) noexcept;

    [[nodiscard]] int getWidth() const noexcept {
        return width;
    }
    [[nodiscard]] int getHeight() const noexcept {
        return height;
    }

    // Returns the sides the cell opens to as a mask of kNorth, kEast, kSouth and kWest. Throws std::out_of_range outside the maze.
    [[nodiscard]] std::uint8_t getOpenings(int x, int y) const;
    // Opens the wall on one side of the cell and the matching wall of its neighbor. Throws std::invalid_argument when the side leads out of the maze.
    void open(int x, int y, std::uint8_t side);
    [[nodiscard]] std::size_t getPassageCount() const noexcept;

    // Draws the maze as 2 * width + 1 by 2 * height + 1 tiles, where 1 marks walls and 0 floors, and cell (x, y) sits on tile (2x + 1, 2y + 1).
    [[nodiscard]] spatial2d::CellGrid toGrid() const;

  private:
    struct Wall {
        int x = 0;
        int y = 0;
        std::uint8_t side = 0;
    };

    [[nodiscard]] static int stepX(std::uint8_t side) noexcept;
    [[nodiscard]] static int stepY(std::uint8_t side) noexcept;
    [[nodiscard]] static std::uint8_t opposite(std::uint8_t side) noexcept;
    [[nodiscard]] bool contains(int x, int y) const noexcept;
    [[nodiscard]] std::size_t indexOf(int x, int y) const noexcept;

    void carveBacktracker(math::Random& random);
    void carvePrim(math::Random& random);
    void carveKruskal(math::Random& random);

    static constexpr std::array<std::uint8_t, 4> kSides{kNorth, kEast, kSouth, kWest};

    int width;
    int height;
    std::vector<std::uint8_t> openings;
};

} // namespace haylen::procedural2d
