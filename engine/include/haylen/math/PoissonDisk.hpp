#pragma once

#include <functional>
#include <vector>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::math {

class Random;

// Poisson disk sampling, which spreads random points evenly over an area.
class PoissonDisk final {
  public:
    // A distance function makes the spacing vary over the area, such as from a density or noise map. Its results are clamped between the minimum and maximum distances, and two points stay at least the larger of their own distances apart.
    struct Options {
        Rect area{};
        float minimumDistance = 64.0F;
        float maximumDistance = 0.0F;
        int attempts = 30;
        std::function<bool(Vec2)> accept;
        std::function<float(Vec2)> distance;
    };

    // Returns evenly spread random points where no two points are closer than the minimum distance. Throws `std::invalid_argument` when a distance or a result of the distance function is not finite, when a distance function comes with a maximum distance below the minimum, or when the minimum distance is so small for the area that the sampling grid would need more than 16777216 cells.
    [[nodiscard]] static std::vector<Vec2> sample(const Options& options, Random& random);

  private:
    class Grid;
};

} // namespace haylen::math
