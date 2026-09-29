#include "haylen/math/PoissonDisk.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "haylen/math/Math.hpp"
#include "haylen/math/Random.hpp"

namespace haylen::math {

// Remembers which sample occupies each cell of a grid whose cells are small enough to hold at most one sample.
class PoissonDisk::Grid final {
  public:
    Grid(const Rect& sampledArea, float size) : area(sampledArea), cellSize(size), columns(cellCount(sampledArea.width, size, sampledArea.height)), rows(cellCount(sampledArea.height, size, sampledArea.width)), cells(static_cast<std::size_t>(columns) * static_cast<std::size_t>(rows), -1) {}

    void insert(Vec2 point, int sampleIndex) {
        cells[cellIndex(columnOf(point), rowOf(point))] = sampleIndex;
    }

    // Returns how many cells away a sample may still be closer than the distance, which never exceeds the size of the grid.
    [[nodiscard]] int reachOf(float distance) const noexcept {
        return static_cast<int>(std::min(std::ceil(static_cast<double>(distance) / cellSize), static_cast<double>(std::max(columns, rows))));
    }

    [[nodiscard]] bool isFarEnough(Vec2 point, float distance, int reach, const std::vector<Vec2>& samples, const std::vector<float>& distances) const {
        const int column = columnOf(point);
        const int row = rowOf(point);

        for (int y = std::max(0, row - reach); y <= std::min(rows - 1, row + reach); ++y) {
            for (int x = std::max(0, column - reach); x <= std::min(columns - 1, column + reach); ++x) {
                const int sample = cells[cellIndex(x, y)];
                if (sample < 0) {
                    continue;
                }
                const auto index = static_cast<std::size_t>(sample);
                const float limit = std::max(distance, distances[index]);
                if (Vec2::distanceSquared(samples[index], point) < limit * limit) {
                    return false;
                }
            }
        }
        return true;
    }

  private:
    static constexpr double kMaxCells = 16777216.0;

    // Counts the cells along one side of the area, refusing grids that no app could hold, whose cell counts would also overflow.
    [[nodiscard]] static int cellCount(float length, float size, float otherLength) {
        const double count = std::max(1.0, std::ceil(static_cast<double>(length) / size));
        const double otherCount = std::max(1.0, std::ceil(static_cast<double>(otherLength) / size));
        if (count * otherCount > kMaxCells) {
            throw std::invalid_argument("The minimum distance is too small for the area, whose sampling grid would need more than 16777216 cells.");
        }
        return static_cast<int>(count);
    }

    [[nodiscard]] int columnOf(Vec2 point) const noexcept {
        return std::clamp(static_cast<int>((point.x - area.x) / cellSize), 0, columns - 1);
    }

    [[nodiscard]] int rowOf(Vec2 point) const noexcept {
        return std::clamp(static_cast<int>((point.y - area.y) / cellSize), 0, rows - 1);
    }

    [[nodiscard]] std::size_t cellIndex(int column, int row) const noexcept {
        return static_cast<std::size_t>(row) * static_cast<std::size_t>(columns) + static_cast<std::size_t>(column);
    }

    Rect area;
    float cellSize;
    int columns;
    int rows;
    std::vector<int> cells;
};

std::vector<Vec2> PoissonDisk::sample(const Options& options, Random& random) {
    std::vector<Vec2> samples;
    if (options.area.isEmpty() || options.minimumDistance <= 0.0F) {
        return samples;
    }

    const bool variable = static_cast<bool>(options.distance);
    const float largest = variable ? options.maximumDistance : options.minimumDistance;
    if (!std::isfinite(options.minimumDistance) || !std::isfinite(largest)) {
        throw std::invalid_argument("Poisson distances must be finite.");
    }
    if (largest < options.minimumDistance) {
        throw std::invalid_argument("A varying Poisson distance needs a maximum distance of at least the minimum distance.");
    }

    Grid grid(options.area, options.minimumDistance / std::sqrt(2.0F));
    const int reach = grid.reachOf(largest);
    std::vector<float> distances;
    std::vector<std::size_t> active;

    // clang-format off
    const auto accepted = [&](Vec2 point) {
        return options.area.contains(point) && (!options.accept || options.accept(point));
    };
    const auto distanceAt = [&](Vec2 point) {
        if (!variable) {
            return options.minimumDistance;
        }
        const float distance = options.distance(point);
        if (!std::isfinite(distance)) {
            throw std::invalid_argument("A Poisson distance function must return finite distances.");
        }
        return std::clamp(distance, options.minimumDistance, largest);
    };
    const auto addSample = [&](Vec2 point, float distance) {
        grid.insert(point, static_cast<int>(samples.size()));
        active.push_back(samples.size());
        samples.push_back(point);
        distances.push_back(distance);
    };
    // clang-format on

    // Seed the process with the first acceptable random point.
    for (int attempt = 0; attempt < options.attemptsPerPoint * 4; ++attempt) {
        const Vec2 seed{random.range(options.area.getLeft(), options.area.getRight()), random.range(options.area.getTop(), options.area.getBottom())};
        if (accepted(seed)) {
            addSample(seed, distanceAt(seed));
            break;
        }
    }

    // Grow outward from active samples until no sample can place a neighbor.
    while (!active.empty()) {
        const auto slot = static_cast<std::size_t>(random.range(0, static_cast<int>(active.size()) - 1));
        const Vec2 origin = samples[active[slot]];
        const float spacing = distances[active[slot]];
        bool placed = false;

        for (int attempt = 0; attempt < options.attemptsPerPoint; ++attempt) {
            const float angle = random.range(0.0F, Math::kTau);
            const float radius = random.range(spacing, spacing * 2.0F);
            const Vec2 candidate = origin + Vec2::fromAngle(angle, radius);
            if (!accepted(candidate)) {
                continue;
            }
            const float distance = distanceAt(candidate);
            if (grid.isFarEnough(candidate, distance, reach, samples, distances)) {
                addSample(candidate, distance);
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
