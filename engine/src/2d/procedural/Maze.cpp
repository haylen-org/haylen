#include "haylen/2d/procedural/Maze.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>

#include "haylen/2d/spatial/UnionFind.hpp"
#include "haylen/math/Random.hpp"

namespace haylen::procedural2d {

const std::array<std::pair<std::string_view, Maze::Algorithm>, 3> Maze::kAlgorithmNames{{{"backtracker", Algorithm::Backtracker}, {"prim", Algorithm::Prim}, {"kruskal", Algorithm::Kruskal}}};

Maze::Maze(int columns, int rows) : width(columns), height(rows) {
    if (columns < 1 || rows < 1 || (2 * std::int64_t{columns} + 1) * (2 * std::int64_t{rows} + 1) > std::numeric_limits<std::int32_t>::max()) {
        throw std::invalid_argument("A maze needs at least one cell on each side and a tile grid that fits in 32-bit cell indices.");
    }
    openings.assign(static_cast<std::size_t>(columns) * static_cast<std::size_t>(rows), 0);
}

std::optional<Maze::Algorithm> Maze::algorithmFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kAlgorithmNames, name, &std::pair<std::string_view, Algorithm>::first);
    return found != kAlgorithmNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view Maze::algorithmName(Algorithm value) noexcept {
    return std::ranges::find(kAlgorithmNames, value, &std::pair<std::string_view, Algorithm>::second)->first;
}

int Maze::stepX(std::uint8_t side) noexcept {
    return side == kEast ? 1 : (side == kWest ? -1 : 0);
}

int Maze::stepY(std::uint8_t side) noexcept {
    return side == kSouth ? 1 : (side == kNorth ? -1 : 0);
}

std::uint8_t Maze::opposite(std::uint8_t side) noexcept {
    return static_cast<std::uint8_t>(side <= kEast ? side << 2U : side >> 2U);
}

bool Maze::contains(int x, int y) const noexcept {
    return x >= 0 && y >= 0 && x < width && y < height;
}

std::size_t Maze::indexOf(int x, int y) const noexcept {
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
}

std::uint8_t Maze::getOpenings(int x, int y) const {
    if (!contains(x, y)) {
        throw std::out_of_range("The cell is outside the maze.");
    }
    return openings[indexOf(x, y)];
}

void Maze::open(int x, int y, std::uint8_t side) {
    const int nextX = x + stepX(side);
    const int nextY = y + stepY(side);
    if (!contains(x, y) || !contains(nextX, nextY) || std::popcount(side) != 1 || side > kWest) {
        throw std::invalid_argument("Only walls between two cells of the maze can open.");
    }
    std::uint8_t& cell = openings[indexOf(x, y)];
    std::uint8_t& neighbor = openings[indexOf(nextX, nextY)];
    cell = static_cast<std::uint8_t>(cell | side);
    neighbor = static_cast<std::uint8_t>(neighbor | opposite(side));
}

std::size_t Maze::getPassageCount() const noexcept {
    std::size_t sides = 0;
    for (const std::uint8_t cell : openings) {
        sides += static_cast<std::size_t>(std::popcount(cell));
    }
    return sides / 2;
}

spatial2d::CellGrid Maze::toGrid() const {
    spatial2d::CellGrid grid(width * 2 + 1, height * 2 + 1, 1);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::uint8_t cell = openings[indexOf(x, y)];
            grid.set({x * 2 + 1, y * 2 + 1}, 0);
            if ((cell & kEast) != 0) {
                grid.set({x * 2 + 2, y * 2 + 1}, 0);
            }
            if ((cell & kSouth) != 0) {
                grid.set({x * 2 + 1, y * 2 + 2}, 0);
            }
        }
    }
    return grid;
}

// Walks forward to random unvisited neighbors and backs up at dead ends, with an explicit stack for large mazes.
void Maze::carveBacktracker(math::Random& random) {
    std::vector<bool> visited(openings.size(), false);
    std::vector<std::pair<int, int>> path{{random.range(0, width - 1), random.range(0, height - 1)}};
    visited[indexOf(path.back().first, path.back().second)] = true;

    while (!path.empty()) {
        const auto [x, y] = path.back();
        std::array<std::uint8_t, 4> choices{};
        std::size_t count = 0;
        for (const std::uint8_t side : kSides) {
            const int nextX = x + stepX(side);
            const int nextY = y + stepY(side);
            if (contains(nextX, nextY) && !visited[indexOf(nextX, nextY)]) {
                choices[count++] = side;
            }
        }

        if (count == 0) {
            path.pop_back();
            continue;
        }
        const std::uint8_t side = choices[static_cast<std::size_t>(random.range(0, static_cast<int>(count) - 1))];
        open(x, y, side);
        visited[indexOf(x + stepX(side), y + stepY(side))] = true;
        path.emplace_back(x + stepX(side), y + stepY(side));
    }
}

// Grows the maze from one cell by opening random walls on its frontier toward cells it has not reached.
void Maze::carvePrim(math::Random& random) {
    std::vector<bool> visited(openings.size(), false);
    std::vector<Wall> frontier;
    // clang-format off
    const auto visit = [&](int x, int y) {
        visited[indexOf(x, y)] = true;
        for (const std::uint8_t side : kSides) {
            if (contains(x + stepX(side), y + stepY(side))) {
                frontier.push_back({x, y, side});
            }
        }
    };
    // clang-format on
    visit(random.range(0, width - 1), random.range(0, height - 1));

    while (!frontier.empty()) {
        const auto slot = static_cast<std::size_t>(random.range(0, static_cast<int>(frontier.size()) - 1));
        const Wall wall = frontier[slot];
        frontier[slot] = frontier.back();
        frontier.pop_back();

        const int nextX = wall.x + stepX(wall.side);
        const int nextY = wall.y + stepY(wall.side);
        if (!visited[indexOf(nextX, nextY)]) {
            open(wall.x, wall.y, wall.side);
            visit(nextX, nextY);
        }
    }
}

// Opens the walls in random order whenever they join two cells that no path joins yet.
void Maze::carveKruskal(math::Random& random) {
    std::vector<Wall> walls;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (x + 1 < width) {
                walls.push_back({x, y, kEast});
            }
            if (y + 1 < height) {
                walls.push_back({x, y, kSouth});
            }
        }
    }
    random.shuffle(std::span<Wall>(walls));

    spatial2d::UnionFind sets(openings.size());
    for (const Wall& wall : walls) {
        if (sets.unite(indexOf(wall.x, wall.y), indexOf(wall.x + stepX(wall.side), wall.y + stepY(wall.side)))) {
            open(wall.x, wall.y, wall.side);
        }
    }
}

Maze Maze::generate(int columns, int rows, Algorithm algorithm, math::Random& random) {
    Maze maze(columns, rows);
    switch (algorithm) {
    case Algorithm::Prim:
        maze.carvePrim(random);
        break;
    case Algorithm::Kruskal:
        maze.carveKruskal(random);
        break;
    case Algorithm::Backtracker:
        maze.carveBacktracker(random);
        break;
    }
    return maze;
}

} // namespace haylen::procedural2d
