#pragma once

#include <array>

#include "haylen/2d/spatial/Cell.hpp"
#include "haylen/2d/spatial/CellGrid.hpp"

namespace haylen::math {
class Random;
}

namespace haylen::procedural2d {

// Carves winding caves with random walkers that start at the center and dig floors until a share of the map is open, where 1 marks walls and 0 floors. The outermost cells stay walls.
class DrunkardWalk final {
  public:
    static constexpr std::int32_t kFloor = 0;
    static constexpr std::int32_t kWall = 1;

    // Coverage is the share of the cells inside the border to open. Walking stops early after maxSteps steps in total.
    struct Options {
        int width = 64;
        int height = 64;
        float coverage = 0.4F;
        int walkers = 1;
        int maxSteps = 100000;
    };

    // Throws std::invalid_argument when a side is below 3 or there are no walkers.
    [[nodiscard]] static spatial2d::CellGrid generate(const Options& options, math::Random& random);

  private:
    static constexpr std::array<spatial2d::Cell, 4> kSteps{{{0, -1}, {1, 0}, {0, 1}, {-1, 0}}};
};

} // namespace haylen::procedural2d
