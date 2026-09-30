#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

#include "haylen/2d/navigation/Grid.hpp"

namespace haylen::navigation2d {

// Finds paths on a navigation grid with A*, weighted A* or jump point search. It keeps its buffers between searches, so a search allocates nothing once they have grown to the grid, and one search object serves any number of grids.
class GridSearch final {
  public:
    struct Options {
        bool diagonal = true;
        bool smooth = false;

        // Estimates the distance left, Octile with diagonal steps and Manhattan without them when unset. Hexagonal grids count hex steps and take none.
        std::optional<Grid::Heuristic> heuristic;

        // Values above 1 trade length for speed, finding paths that cost at most `weight` times the cheapest one.
        float weight = 1.0F;

        // Jump point search skips over runs of open ground and finds paths as cheap as A* while expanding far fewer cells, on square and staggered grids where every cell costs 1.
        bool jumpPoint = false;
    };

    // Returns the cells from start to goal, both included, or an empty path when either end is blocked or the goal is unreachable. The path stays valid until the next search.
    std::span<const Grid::Cell> findPath(const Grid& grid, Grid::Cell start, Grid::Cell goal, const Options& options = kDefaultOptions);

    // Returns the cost of the last path found, which is infinite when there was none.
    [[nodiscard]] float getCost() const noexcept {
        return cost;
    }

    // Returns how many cells the last search expanded, which shows how much work a heuristic or jump point search saves.
    [[nodiscard]] std::size_t getExpandedCount() const noexcept {
        return expanded;
    }

    // Throws `std::invalid_argument` when the options do not suit the grid, which lets background searches reject bad arguments before they start.
    static void requireValid(const Grid& grid, const Options& options);

  private:
    struct OpenNode {
        float estimate = 0.0F;
        float heuristic = 0.0F;
        std::int32_t cell = 0;
    };

    struct Node {
        float score = std::numeric_limits<float>::infinity();
        std::int32_t parent = -1;
        std::uint32_t visit = 0;
        bool closed = false;
    };

    static const Options kDefaultOptions;

    // Orders the open set by estimated total cost, then by distance left, then by cell index, so equal paths always resolve the same way.
    [[nodiscard]] static bool isWorse(const OpenNode& lhs, const OpenNode& rhs) noexcept;

    void begin(std::size_t cells);
    [[nodiscard]] Node& at(std::int32_t cell);
    void push(const OpenNode& node);
    [[nodiscard]] OpenNode pop();

    void searchAStar(const Grid& grid, Grid::Cell start, Grid::Cell goal, const Options& options, Grid::Heuristic heuristic);
    void searchJumpPoints(const Grid& grid, Grid::Cell start, Grid::Cell goal, const Options& options, Grid::Heuristic heuristic);

    // Fills `candidates` with the lattice points worth exploring from a point reached from its parent, the neighbors jump point search keeps, and returns how many there are.
    [[nodiscard]] static std::size_t prunedNeighbors(const Grid& grid, Grid::Cell point, std::optional<Grid::Cell> parent, bool diagonal, std::array<Grid::Cell, 8>& candidates);

    // Moves from a lattice point along a direction until it finds a jump point, the goal or a wall.
    [[nodiscard]] static std::optional<Grid::Cell> jump(const Grid& grid, Grid::Cell point, Grid::Cell direction, Grid::Cell goal, bool diagonal);
    void buildPath(const Grid& grid, std::int32_t goal, bool jumpPoints);

    std::vector<Node> nodes;
    std::uint32_t visit = 0;
    std::vector<OpenNode> open;
    std::vector<Grid::Cell> path;
    std::vector<Grid::Cell> jumps;
    float cost = std::numeric_limits<float>::infinity();
    std::size_t expanded = 0;
};

} // namespace haylen::navigation2d
