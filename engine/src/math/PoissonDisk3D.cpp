#include "haylen/math/PoissonDisk3D.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

#include "haylen/math/Random.hpp"

namespace haylen::math {

// The algorithm of Bridson in three dimensions: a grid whose cells hold at most one sample, a seed point, and new points tried in the shell between one and two distances around a random active point until none fits.
std::vector<Vec3> PoissonDisk3D::sample(const Options& options, Random& random) {
    std::vector<Vec3> samples;
    const Box& volume = options.volume;
    if (!std::isfinite(options.minimumDistance) || !volume.min.isFinite() || !volume.max.isFinite()) {
        throw std::invalid_argument("Poisson sampling in 3D needs a finite distance and a box whose sides are finite.");
    }

    if (options.attempts < 1 || options.attempts > kMaxAttempts) {
        throw std::invalid_argument("Poisson sampling needs between 1 and 1000 attempts.");
    }

    if (volume.isEmpty() || options.minimumDistance <= 0.0F) {
        return samples;
    }

    const float cellSize = options.minimumDistance / std::sqrt(3.0F);
    const Vec3 size = volume.getSize();
    const std::array<double, 3> counts{std::max(1.0, std::ceil(static_cast<double>(size.x) / cellSize)), std::max(1.0, std::ceil(static_cast<double>(size.y) / cellSize)), std::max(1.0, std::ceil(static_cast<double>(size.z) / cellSize))};
    if (counts[0] * counts[1] * counts[2] > kMaxCells) {
        throw std::invalid_argument("The minimum distance is too small for the box, whose sampling grid would need more than 16777216 cells.");
    }

    const std::array<int, 3> cells{static_cast<int>(counts[0]), static_cast<int>(counts[1]), static_cast<int>(counts[2])};
    std::vector<int> grid(static_cast<std::size_t>(cells[0]) * static_cast<std::size_t>(cells[1]) * static_cast<std::size_t>(cells[2]), -1);
    std::vector<std::size_t> active;

    // clang-format off
    const auto cellOf = [&](Vec3 point) {
        const Vec3 offset = (point - volume.min) / cellSize;
        return std::array<int, 3>{std::clamp(static_cast<int>(offset.x), 0, cells[0] - 1), std::clamp(static_cast<int>(offset.y), 0, cells[1] - 1), std::clamp(static_cast<int>(offset.z), 0, cells[2] - 1)};
    };
    const auto indexOf = [&](const std::array<int, 3>& cell) {
        return (static_cast<std::size_t>(cell[2]) * static_cast<std::size_t>(cells[1]) + static_cast<std::size_t>(cell[1])) * static_cast<std::size_t>(cells[0]) + static_cast<std::size_t>(cell[0]);
    };
    const auto accepted = [&](Vec3 point) {
        return volume.contains(point) && (!options.accept || options.accept(point));
    };
    const auto isFarEnough = [&](Vec3 point) {
        const std::array<int, 3> center = cellOf(point);
        for (int z = std::max(0, center[2] - 2); z <= std::min(cells[2] - 1, center[2] + 2); ++z) {
            for (int y = std::max(0, center[1] - 2); y <= std::min(cells[1] - 1, center[1] + 2); ++y) {
                for (int x = std::max(0, center[0] - 2); x <= std::min(cells[0] - 1, center[0] + 2); ++x) {
                    const int sample = grid[indexOf({x, y, z})];
                    if (sample >= 0 && Vec3::distanceSquared(samples[static_cast<std::size_t>(sample)], point) < options.minimumDistance * options.minimumDistance) {
                        return false;
                    }
                }
            }
        }

        return true;
    };
    const auto addSample = [&](Vec3 point) {
        grid[indexOf(cellOf(point))] = static_cast<int>(samples.size());
        active.push_back(samples.size());
        samples.push_back(point);
    };
    // clang-format on

    for (int attempt = 0; attempt < options.attempts * 4; ++attempt) {
        const Vec3 seed = random.inBox(volume);
        if (accepted(seed)) {
            addSample(seed);
            break;
        }
    }

    // A point of the shell spreads evenly through its volume when its radius cubed is even between the cubes of the inner and outer radii.
    while (!active.empty()) {
        const auto slot = static_cast<std::size_t>(random.range(0, static_cast<int>(active.size()) - 1));
        const Vec3 origin = samples[active[slot]];
        bool placed = false;
        for (int attempt = 0; attempt < options.attempts; ++attempt) {
            const float radius = options.minimumDistance * std::cbrt(random.range(1.0F, 8.0F));
            const Vec3 candidate = origin + random.onSphere(radius);
            if (accepted(candidate) && isFarEnough(candidate)) {
                addSample(candidate);
                placed = true;
                break;
            }
        }

        if (!placed) {
            active[slot] = active.back();
            active.pop_back();
        }
    }

    return samples;
}

} // namespace haylen::math
