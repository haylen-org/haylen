#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "haylen/2d/navigation/DijkstraMap.hpp"
#include "haylen/2d/navigation/Grid.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::navigation2d {

// Steps toward the nearest of several goals for every cell of a navigation grid, computed once for any number of units, as in real-time strategy games and tower defense. Each unit reads the step of its cell in constant time. It keeps its buffers between computations.
class FlowField final {
  public:
    void compute(const Grid& grid, std::span<const Grid::Cell> goals, bool diagonal = true);

    // Returns the cell to step into along a cheapest path, or nothing at a goal and at blocked or unreachable cells.
    [[nodiscard]] std::optional<Grid::Cell> getNext(Grid::Cell cell) const;

    // Returns the unit direction from the cell toward its next cell in grid coordinates, which is the world direction on square grids, or zero where there is no next cell.
    [[nodiscard]] math::Vec2 getDirection(Grid::Cell cell) const;

    // Returns the cost of the cheapest walk from the cell to a goal, which is infinite when no goal can be reached.
    [[nodiscard]] float getDistance(Grid::Cell cell) const;

    [[nodiscard]] int getWidth() const noexcept {
        return distances.getWidth();
    }
    [[nodiscard]] int getHeight() const noexcept {
        return distances.getHeight();
    }

  private:
    [[nodiscard]] std::size_t indexOf(Grid::Cell cell) const;

    DijkstraMap distances;
    std::vector<DijkstraMap::Source> sources;
    std::vector<std::int32_t> next;
};

} // namespace haylen::navigation2d
