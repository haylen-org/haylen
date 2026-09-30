#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <vector>

namespace haylen::navigation2d {

// A walkability grid with per-cell costs for path finding, laid out like the maps of Tiled. Cells start walkable with cost 1, and cells outside the grid are blocked. Cells are addressed by column and row like Tiled cells, so the cells of a hexagonal or staggered map are its offset coordinates.
class Grid final {
  public:
    struct Cell {
        int x = 0;
        int y = 0;

        [[nodiscard]] bool operator==(const Cell&) const noexcept = default;
    };

    enum class Topology : std::uint8_t {
        // Cells with four sides and four corners, shared by orthogonal, isometric and oblique maps.
        Square,
        // Hexagons in shifted rows, or in shifted columns when the stagger axis is x.
        Hexagonal,
        // Diamonds in shifted rows, or in shifted columns when the stagger axis is x.
        Staggered,
    };

    // The stagger fields follow the map properties of Tiled: the axis whose rows or columns shift, and whether the even ones shift instead of the odd ones.
    struct Layout {
        Topology topology = Topology::Square;
        bool staggerX = false;
        bool staggerEven = false;
    };

    // How a search estimates the distance left. Octile is exact on open ground with diagonal steps and Manhattan without them, Euclidean and Chebyshev underestimate more, and hexagonal grids always count hex steps. Manhattan overestimates walks with diagonal steps, so its path may not be the cheapest one.
    enum class Heuristic : std::uint8_t {
        Manhattan,
        Octile,
        Euclidean,
        Chebyshev,
    };

    static constexpr float kDiagonalStep = std::numbers::sqrt2_v<float>;

    Grid(int columns, int rows, const Layout& value = kDefaultLayout);

    [[nodiscard]] int getWidth() const noexcept {
        return width;
    }
    [[nodiscard]] int getHeight() const noexcept {
        return height;
    }
    [[nodiscard]] const Layout& getLayout() const noexcept {
        return layout;
    }
    [[nodiscard]] bool contains(Cell cell) const noexcept {
        return cell.x >= 0 && cell.y >= 0 && cell.x < width && cell.y < height;
    }
    [[nodiscard]] std::size_t indexOf(Cell cell) const noexcept {
        return static_cast<std::size_t>(cell.y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(cell.x);
    }
    [[nodiscard]] Cell cellAt(std::size_t index) const noexcept {
        return {static_cast<int>(index % static_cast<std::size_t>(width)), static_cast<int>(index / static_cast<std::size_t>(width))};
    }
    [[nodiscard]] std::size_t getCellCount() const noexcept {
        return walkable.size();
    }

    void setWalkable(Cell cell, bool value);
    [[nodiscard]] bool isWalkable(Cell cell) const noexcept {
        return contains(cell) && walkable[indexOf(cell)] != 0;
    }

    // Scales the cost of entering the cell, with 1 for plain ground. Costs below 1 are rejected so searches stay optimal.
    void setCost(Cell cell, float value);
    [[nodiscard]] float getCost(Cell cell) const;
    [[nodiscard]] float getCostAt(std::size_t index) const noexcept {
        return costs[index];
    }

    // Tells whether every cell costs 1, which jump point search needs.
    [[nodiscard]] bool hasUniformCost() const noexcept {
        return costlyCells == 0;
    }

    // Calls `visit(cell, length)` for every walkable cell a walker can step into from the cell, with the length of the step. Square and staggered grids step through the four sides, and with diagonal steps also through the four corners when both cells beside a corner are walkable, so paths never cut corners. Hexagonal grids step through their six sides whatever `diagonal` says.
    template <typename Visit> void forEachStep(Cell cell, bool diagonal, Visit&& visit) const {
        if (layout.topology == Topology::Hexagonal) {
            const Cell axial = toAxial(cell);
            for (const Cell direction : kHexDirections) {
                const Cell next = fromAxial({axial.x + direction.x, axial.y + direction.y});
                if (isWalkable(next)) {
                    visit(next, 1.0F);
                }
            }
            return;
        }

        const Cell point = toLattice(cell);
        const std::size_t directions = diagonal ? kLatticeDirections.size() : 4;
        for (std::size_t direction = 0; direction < directions; ++direction) {
            const Cell step = kLatticeDirections[direction];
            const Cell next = fromLattice({point.x + step.x, point.y + step.y});
            if (!isWalkable(next)) {
                continue;
            }
            const bool corner = step.x != 0 && step.y != 0;
            if (corner && (!isWalkable(fromLattice({point.x + step.x, point.y})) || !isWalkable(fromLattice({point.x, point.y + step.y})))) {
                continue;
            }
            visit(next, corner ? kDiagonalStep : 1.0F);
        }
    }

    // Estimates the length of the walk between two cells, ignoring walls and costs.
    [[nodiscard]] float estimate(Cell from, Cell to, Heuristic heuristic) const noexcept;

    // Square and staggered grids map onto a square lattice whose four axis steps cross the sides of the cells, which jump point search and lines of sight walk. Square grids map onto themselves, and hexagonal grids have no lattice.
    [[nodiscard]] Cell toLattice(Cell cell) const noexcept;
    [[nodiscard]] Cell fromLattice(Cell point) const noexcept;

    // Returns `true` when the straight line between both cell centers crosses only walkable cells.
    [[nodiscard]] bool hasLineOfSight(Cell from, Cell to) const noexcept;

    // Keeps the start, the goal and every cell where the path must turn, dropping in place the cells a straight walk can skip. Smoothing looks at walkability only, so it may cross costly cells.
    void smoothPath(std::vector<Cell>& path) const;

  private:
    static const Layout kDefaultLayout;
    static constexpr std::array<Cell, 8> kLatticeDirections{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}}};
    static constexpr std::array<Cell, 6> kHexDirections{{{1, 0}, {1, -1}, {0, -1}, {-1, 0}, {-1, 1}, {0, 1}}};

    [[nodiscard]] static int positiveModulo(int value, int divisor) noexcept {
        return ((value % divisor) + divisor) % divisor;
    }

    // Hexagonal cells convert to and from axial coordinates, where the six sides are the six unit steps.
    [[nodiscard]] Cell toAxial(Cell cell) const noexcept;
    [[nodiscard]] Cell fromAxial(Cell axial) const noexcept;

    // Maps a staggered cell given as a shifted row and a position along it to the lattice, and back.
    [[nodiscard]] Cell staggeredToLattice(int along, int row) const noexcept;
    [[nodiscard]] Cell staggeredFromLattice(Cell point) const noexcept;

    [[nodiscard]] bool hasLatticeLineOfSight(Cell from, Cell to) const noexcept;
    [[nodiscard]] bool hasHexLineOfSight(Cell from, Cell to) const noexcept;
    void requireInside(Cell cell) const;

    int width;
    int height;
    Layout layout;
    std::vector<std::uint8_t> walkable;
    std::vector<float> costs;
    std::size_t costlyCells = 0;
};

} // namespace haylen::navigation2d
