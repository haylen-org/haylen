#include "haylen/2d/procedural/DrunkardWalk.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "haylen/math/Math.hpp"
#include "haylen/math/Random.hpp"

namespace haylen::procedural2d {

spatial2d::CellGrid DrunkardWalk::generate(const Options& options, math::Random& random) {
    if (options.width < 3 || options.height < 3 || options.walkers < 1) {
        throw std::invalid_argument("A drunkard walk needs a map of at least 3 by 3 cells and at least one walker.");
    }

    spatial2d::CellGrid grid(options.width, options.height, kWall);
    const auto inner = static_cast<std::size_t>(options.width - 2) * static_cast<std::size_t>(options.height - 2);
    const auto target = static_cast<std::size_t>(std::ceil(static_cast<float>(inner) * math::Math::saturate(options.coverage)));
    std::vector<spatial2d::Cell> walkers(static_cast<std::size_t>(options.walkers), spatial2d::Cell{options.width / 2, options.height / 2});

    std::size_t open = 0;
    for (int step = 0; step < options.maxSteps && open < target; ++step) {
        spatial2d::Cell& walker = walkers[static_cast<std::size_t>(step) % walkers.size()];
        if (grid[walker] == kWall) {
            grid.set(walker, kFloor);
            ++open;
        }

        const spatial2d::Cell move = kSteps[static_cast<std::size_t>(random.range(0, 3))];
        walker.x = std::clamp(walker.x + move.x, 1, options.width - 2);
        walker.y = std::clamp(walker.y + move.y, 1, options.height - 2);
    }
    return grid;
}

} // namespace haylen::procedural2d
