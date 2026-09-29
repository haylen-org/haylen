#include "haylen/2d/procedural/WaveFunctionCollapse.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>

#include "haylen/math/Random.hpp"

namespace haylen::procedural2d {

WaveFunctionCollapse::Rules::Rules(std::size_t tiles) : tileCount(tiles), weights(tiles, 1.0F) {
    if (tiles == 0) {
        throw std::invalid_argument("Wave function collapse needs at least one tile.");
    }
    for (std::vector<bool>& pairs : allowed) {
        pairs.assign(tiles * tiles, false);
    }
}

void WaveFunctionCollapse::Rules::requireTile(std::size_t tile) const {
    if (tile >= tileCount) {
        throw std::out_of_range("The rules have no tile " + std::to_string(tile) + ".");
    }
}

void WaveFunctionCollapse::Rules::allow(std::size_t first, std::size_t second, Direction direction) {
    requireTile(first);
    requireTile(second);
    allowed[static_cast<std::size_t>(direction)][first * tileCount + second] = true;
    allowed[static_cast<std::size_t>(opposite(direction))][second * tileCount + first] = true;
}

void WaveFunctionCollapse::Rules::setWeight(std::size_t tile, float weight) {
    requireTile(tile);
    if (!(weight >= 0.0F)) {
        throw std::invalid_argument("Tile weights cannot be negative.");
    }
    weights[tile] = weight;
}

bool WaveFunctionCollapse::Rules::isAllowed(std::size_t first, std::size_t second, Direction direction) const noexcept {
    return first < tileCount && second < tileCount && allowed[static_cast<std::size_t>(direction)][first * tileCount + second];
}

float WaveFunctionCollapse::Rules::getWeight(std::size_t tile) const noexcept {
    return tile < tileCount ? weights[tile] : 0.0F;
}

WaveFunctionCollapse::Rules WaveFunctionCollapse::Rules::fromSample(const spatial2d::CellGrid& sample, bool periodic) {
    const std::span<const std::int32_t> cells = sample.getValues();
    if (*std::min_element(cells.begin(), cells.end()) < 0) {
        throw std::invalid_argument("Every cell of a sample must hold a tile number of at least 0.");
    }

    Rules rules(static_cast<std::size_t>(*std::max_element(cells.begin(), cells.end())) + 1);
    std::fill(rules.weights.begin(), rules.weights.end(), 0.0F);
    const int width = sample.getWidth();
    const int height = sample.getHeight();
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto tile = static_cast<std::size_t>(sample[{x, y}]);
            rules.weights[tile] += 1.0F;
            if (x + 1 < width || periodic) {
                rules.allow(tile, static_cast<std::size_t>(sample[{(x + 1) % width, y}]), Direction::Right);
            }
            if (y + 1 < height || periodic) {
                rules.allow(tile, static_cast<std::size_t>(sample[{x, (y + 1) % height}]), Direction::Down);
            }
        }
    }
    return rules;
}

WaveFunctionCollapse::Direction WaveFunctionCollapse::opposite(Direction direction) noexcept {
    return static_cast<Direction>((static_cast<int>(direction) + 2) % 4);
}

// Keeps the tiles each cell may still take, and for every cell, tile and side how many tiles of the neighbor on that side still allow it. A tile whose count drops to zero on any side is banned, and bans spread from cell to cell.
class WaveFunctionCollapse::Solver final {
  public:
    Solver(const Rules& rules, const Options& settings) : options(settings), tileCount(rules.getTileCount()), cellCount(static_cast<std::size_t>(settings.width) * static_cast<std::size_t>(settings.height)) {
        for (std::size_t direction = 0; direction < 4; ++direction) {
            propagator[direction].resize(tileCount);
            for (std::size_t first = 0; first < tileCount; ++first) {
                for (std::size_t second = 0; second < tileCount; ++second) {
                    if (rules.isAllowed(first, second, static_cast<Direction>(direction))) {
                        propagator[direction][first].push_back(static_cast<std::uint32_t>(second));
                    }
                }
            }
        }
        for (std::size_t tile = 0; tile < tileCount; ++tile) {
            const double weight = static_cast<double>(rules.getWeight(tile));
            weights.push_back(weight);
            weightLogWeights.push_back(weight > 0.0 ? weight * std::log(weight) : 0.0);
        }
        wave.resize(cellCount * tileCount);
        compatible.resize(cellCount * tileCount * 4);
        possibleCounts.resize(cellCount);
        sumWeights.resize(cellCount);
        sumWeightLogWeights.resize(cellCount);
        noise.resize(cellCount);
    }

    [[nodiscard]] std::optional<spatial2d::CellGrid> run(math::Random& random) {
        for (int attempt = 0; attempt < options.attempts; ++attempt) {
            reset(random);
            if (!constrain()) {
                return std::nullopt;
            }
            while (true) {
                const std::optional<std::size_t> cell = findLowestEntropy();
                if (contradiction) {
                    break;
                }
                if (!cell) {
                    return toGrid();
                }
                collapse(*cell, random);
                if (!propagate()) {
                    break;
                }
            }
        }
        return std::nullopt;
    }

  private:
    void reset(math::Random& random) {
        std::fill(wave.begin(), wave.end(), std::uint8_t{1});
        for (std::size_t cell = 0; cell < cellCount; ++cell) {
            for (std::size_t tile = 0; tile < tileCount; ++tile) {
                for (std::size_t direction = 0; direction < 4; ++direction) {
                    compatible[(cell * tileCount + tile) * 4 + direction] = static_cast<std::int32_t>(propagator[(direction + 2) % 4][tile].size());
                }
            }
            possibleCounts[cell] = tileCount;
            noise[cell] = static_cast<double>(random.nextFloat()) * 1e-6;
        }
        double total = 0.0;
        double totalLog = 0.0;
        for (std::size_t tile = 0; tile < tileCount; ++tile) {
            total += weights[tile];
            totalLog += weightLogWeights[tile];
        }
        std::fill(sumWeights.begin(), sumWeights.end(), total);
        std::fill(sumWeightLogWeights.begin(), sumWeightLogWeights.end(), totalLog);
        stack.clear();
        contradiction = false;
    }

    // Bans the tiles that can never appear, then applies the fixed cells.
    [[nodiscard]] bool constrain() {
        for (std::size_t tile = 0; tile < tileCount; ++tile) {
            if (weights[tile] <= 0.0) {
                for (std::size_t cell = 0; cell < cellCount; ++cell) {
                    ban(cell, tile);
                }
            }
        }
        if (options.fixed) {
            const std::span<const std::int32_t> fixed = options.fixed->getValues();
            for (std::size_t cell = 0; cell < cellCount; ++cell) {
                if (fixed[cell] < 0) {
                    continue;
                }
                for (std::size_t tile = 0; tile < tileCount; ++tile) {
                    if (tile != static_cast<std::size_t>(fixed[cell])) {
                        ban(cell, tile);
                    }
                }
            }
        }
        return propagate() && !contradiction;
    }

    void ban(std::size_t cell, std::size_t tile) {
        std::uint8_t& possible = wave[cell * tileCount + tile];
        if (possible == 0) {
            return;
        }
        possible = 0;
        for (std::size_t direction = 0; direction < 4; ++direction) {
            compatible[(cell * tileCount + tile) * 4 + direction] = 0;
        }
        stack.emplace_back(static_cast<std::uint32_t>(cell), static_cast<std::uint32_t>(tile));
        sumWeights[cell] -= weights[tile];
        sumWeightLogWeights[cell] -= weightLogWeights[tile];
        if (--possibleCounts[cell] == 0) {
            contradiction = true;
        }
    }

    // Returns the unsettled cell with the lowest entropy, or nothing when every cell is settled or one has no tile left.
    [[nodiscard]] std::optional<std::size_t> findLowestEntropy() {
        std::optional<std::size_t> lowest;
        double lowestEntropy = std::numeric_limits<double>::infinity();
        for (std::size_t cell = 0; cell < cellCount; ++cell) {
            if (possibleCounts[cell] == 0) {
                contradiction = true;
                return std::nullopt;
            }
            if (possibleCounts[cell] == 1) {
                continue;
            }
            const double entropy = std::log(sumWeights[cell]) - sumWeightLogWeights[cell] / sumWeights[cell] + noise[cell];
            if (entropy < lowestEntropy) {
                lowestEntropy = entropy;
                lowest = cell;
            }
        }
        return lowest;
    }

    void collapse(std::size_t cell, math::Random& random) {
        double target = static_cast<double>(random.nextFloat()) * sumWeights[cell];
        std::size_t chosen = tileCount;
        for (std::size_t tile = 0; tile < tileCount; ++tile) {
            if (wave[cell * tileCount + tile] == 0) {
                continue;
            }
            chosen = tile;
            target -= weights[tile];
            if (target < 0.0) {
                break;
            }
        }
        for (std::size_t tile = 0; tile < tileCount; ++tile) {
            if (tile != chosen) {
                ban(cell, tile);
            }
        }
    }

    [[nodiscard]] bool propagate() {
        while (!stack.empty() && !contradiction) {
            const auto [cell, tile] = stack.back();
            stack.pop_back();
            const int x = static_cast<int>(cell % static_cast<std::size_t>(options.width));
            const int y = static_cast<int>(cell / static_cast<std::size_t>(options.width));

            for (std::size_t direction = 0; direction < 4; ++direction) {
                int nextX = x + kStepX[direction];
                int nextY = y + kStepY[direction];
                if (options.periodic) {
                    nextX = (nextX + options.width) % options.width;
                    nextY = (nextY + options.height) % options.height;
                } else if (nextX < 0 || nextY < 0 || nextX >= options.width || nextY >= options.height) {
                    continue;
                }

                const std::size_t neighbor = static_cast<std::size_t>(nextY) * static_cast<std::size_t>(options.width) + static_cast<std::size_t>(nextX);
                for (const std::uint32_t supported : propagator[direction][tile]) {
                    std::int32_t& count = compatible[(neighbor * tileCount + supported) * 4 + direction];
                    if (--count == 0) {
                        ban(neighbor, supported);
                    }
                }
            }
        }
        return !contradiction;
    }

    [[nodiscard]] spatial2d::CellGrid toGrid() const {
        spatial2d::CellGrid grid(options.width, options.height);
        for (std::size_t cell = 0; cell < cellCount; ++cell) {
            const auto first = wave.begin() + static_cast<std::ptrdiff_t>(cell * tileCount);
            const auto tile = std::find(first, first + static_cast<std::ptrdiff_t>(tileCount), std::uint8_t{1}) - first;
            grid.set({static_cast<int>(cell % static_cast<std::size_t>(options.width)), static_cast<int>(cell / static_cast<std::size_t>(options.width))}, static_cast<std::int32_t>(tile));
        }
        return grid;
    }

    static constexpr std::array<int, 4> kStepX{1, 0, -1, 0};
    static constexpr std::array<int, 4> kStepY{0, 1, 0, -1};

    const Options& options;
    std::size_t tileCount;
    std::size_t cellCount;
    std::array<std::vector<std::vector<std::uint32_t>>, 4> propagator;
    std::vector<double> weights;
    std::vector<double> weightLogWeights;
    std::vector<std::uint8_t> wave;
    std::vector<std::int32_t> compatible;
    std::vector<std::size_t> possibleCounts;
    std::vector<double> sumWeights;
    std::vector<double> sumWeightLogWeights;
    std::vector<double> noise;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> stack;
    bool contradiction = false;
};

std::optional<spatial2d::CellGrid> WaveFunctionCollapse::generate(const Rules& rules, const Options& options, math::Random& random) {
    if (options.width < 1 || options.height < 1) {
        throw std::invalid_argument("Wave function collapse needs at least one cell on each side.");
    }
    if (options.fixed && (options.fixed->getWidth() != options.width || options.fixed->getHeight() != options.height)) {
        throw std::invalid_argument("The fixed cells must match the size of the output.");
    }
    return Solver(rules, options).run(random);
}

bool WaveFunctionCollapse::isValid(const spatial2d::CellGrid& tiles, const Rules& rules, bool periodic) noexcept {
    const int width = tiles.getWidth();
    const int height = tiles.getHeight();
    // clang-format off
    const auto allows = [&](std::int32_t first, std::int32_t second, Direction direction) {
        return first >= 0 && second >= 0 && rules.isAllowed(static_cast<std::size_t>(first), static_cast<std::size_t>(second), direction);
    };
    // clang-format on
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::int32_t tile = tiles[{x, y}];
            if (tile < 0 || static_cast<std::size_t>(tile) >= rules.getTileCount()) {
                return false;
            }
            if ((x + 1 < width || periodic) && !allows(tile, tiles[{(x + 1) % width, y}], Direction::Right)) {
                return false;
            }
            if ((y + 1 < height || periodic) && !allows(tile, tiles[{x, (y + 1) % height}], Direction::Down)) {
                return false;
            }
        }
    }
    return true;
}

} // namespace haylen::procedural2d
