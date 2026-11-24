#pragma once

#include <functional>
#include <vector>

#include "haylen/math/Box.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::math {

class Random;

// Poisson disk sampling in 3D, which spreads random points evenly through a box, such as trees on a terrain seen from above, stars in a nebula or the seeds of a cave.
class PoissonDisk3D final {
  public:
    struct Options {
        Box volume{};
        float minimumDistance = 1.0F;
        int attempts = 30;
        std::function<bool(Vec3)> accept{};
    };

    // Returns evenly spread random points of the box where no two points are closer than the minimum distance, and an empty list for an empty box or a distance that is not above 0. Throws `std::invalid_argument` when the distance or a side of the box is not finite, when the attempts are not between 1 and 1000, or when the distance is so small for the box that the sampling grid would need more than 16777216 cells.
    [[nodiscard]] static std::vector<Vec3> sample(const Options& options, Random& random);

  private:
    static constexpr int kMaxAttempts = 1000;
    static constexpr double kMaxCells = 16777216.0;
};

} // namespace haylen::math
