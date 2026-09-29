#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "haylen/2d/navigation/Grid.hpp"

namespace haylen::navigation2d {

// Measures how far every cell of a navigation grid is from the nearest of several sources, the Dijkstra map of roguelikes. Walking downhill from any cell follows a cheapest path to a source, and a flee map made from it leads away from the sources. It keeps its buffers between computations.
class DijkstraMap final {
  public:
    struct Source {
        Grid::Cell cell;
        float value = 0.0F;
    };

    // Stores for every cell the cost of the cheapest walk to a source plus the value of that source, so sources with lower values attract more. Blocked and unreachable cells get infinity, and so do sources on blocked cells.
    void compute(const Grid& grid, std::span<const Source> sources, bool diagonal = true);

    // Turns the map into a flee map. Every value is multiplied by the coefficient, which must be negative, and the walk runs again, so the cells farthest from the sources become the lowest while dead ends stay higher. Around -1.2 makes walkers prefer open escape routes to corners.
    void flee(const Grid& grid, float coefficient = -1.2F);

    [[nodiscard]] float getValue(Grid::Cell cell) const;

    // Returns the neighbor that leads downhill along a cheapest path, or nothing at a lowest point or outside the reachable cells.
    [[nodiscard]] std::optional<Grid::Cell> getNext(const Grid& grid, Grid::Cell cell) const;

    [[nodiscard]] int getWidth() const noexcept {
        return width;
    }
    [[nodiscard]] int getHeight() const noexcept {
        return height;
    }
    [[nodiscard]] bool isDiagonal() const noexcept {
        return diagonal;
    }
    [[nodiscard]] std::span<const float> getValues() const noexcept {
        return values;
    }

  private:
    struct OpenNode {
        float value = 0.0F;
        std::int32_t cell = 0;
    };

    [[nodiscard]] static bool isWorse(const OpenNode& lhs, const OpenNode& rhs) noexcept;
    void requireGrid(const Grid& grid) const;

    // Lowers every value to the cheapest one reachable from the values already set, walking the steps backwards from each cell.
    void walk(const Grid& grid);

    std::vector<float> values;
    std::vector<OpenNode> open;
    int width = 0;
    int height = 0;
    bool diagonal = true;
};

} // namespace haylen::navigation2d
