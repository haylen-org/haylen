#include "haylen/2d/navigation/GridSearch.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <stdexcept>

namespace haylen::navigation2d {

const GridSearch::Options GridSearch::kDefaultOptions{};

bool GridSearch::isWorse(const OpenNode& lhs, const OpenNode& rhs) noexcept {
    if (lhs.estimate != rhs.estimate) {
        return lhs.estimate > rhs.estimate;
    }
    if (lhs.heuristic != rhs.heuristic) {
        return lhs.heuristic > rhs.heuristic;
    }
    return lhs.cell > rhs.cell;
}

void GridSearch::requireValid(const Grid& grid, const Options& options) {
    if (!(options.weight >= 1.0F) || !std::isfinite(options.weight)) {
        throw std::invalid_argument("A search weight must be finite and at least 1.");
    }
    const bool hexagonal = grid.getLayout().topology == Grid::Topology::Hexagonal;
    if (hexagonal && options.heuristic) {
        throw std::invalid_argument("Hexagonal grids count hex steps and take no heuristic.");
    }
    if (options.jumpPoint && hexagonal) {
        throw std::invalid_argument("Jump point search needs a square or staggered grid.");
    }
    if (options.jumpPoint && !grid.hasUniformCost()) {
        throw std::invalid_argument("Jump point search needs a grid where every cell costs 1.");
    }
}

void GridSearch::begin(std::size_t cells) {
    if (nodes.size() < cells) {
        nodes.resize(cells);
    }
    ++visit;
    if (visit == 0) {
        std::ranges::fill(nodes, Node{});
        visit = 1;
    }
    open.clear();
}

GridSearch::Node& GridSearch::at(std::int32_t cell) {
    Node& node = nodes[static_cast<std::size_t>(cell)];
    if (node.visit != visit) {
        node = {.visit = visit};
    }
    return node;
}

void GridSearch::push(const OpenNode& node) {
    open.push_back(node);
    std::ranges::push_heap(open, &GridSearch::isWorse);
}

GridSearch::OpenNode GridSearch::pop() {
    std::ranges::pop_heap(open, &GridSearch::isWorse);
    const OpenNode node = open.back();
    open.pop_back();
    return node;
}

std::span<const Grid::Cell> GridSearch::findPath(const Grid& grid, Grid::Cell start, Grid::Cell goal, const Options& options) {
    requireValid(grid, options);
    path.clear();
    cost = std::numeric_limits<float>::infinity();
    expanded = 0;
    if (!grid.isWalkable(start) || !grid.isWalkable(goal)) {
        return {};
    }
    if (start == goal) {
        path.push_back(start);
        cost = 0.0F;
        return path;
    }

    begin(grid.getCellCount());
    const Grid::Heuristic heuristic = options.heuristic.value_or(options.diagonal ? Grid::Heuristic::Octile : Grid::Heuristic::Manhattan);
    if (options.jumpPoint) {
        searchJumpPoints(grid, start, goal, options, heuristic);
    } else {
        searchAStar(grid, start, goal, options, heuristic);
    }

    const auto goalIndex = static_cast<std::int32_t>(grid.indexOf(goal));
    if (!at(goalIndex).closed) {
        return {};
    }
    cost = at(goalIndex).score;
    buildPath(grid, goalIndex, options.jumpPoint);
    if (options.smooth) {
        grid.smoothPath(path);
    }
    return path;
}

void GridSearch::searchAStar(const Grid& grid, Grid::Cell start, Grid::Cell goal, const Options& options, Grid::Heuristic heuristic) {
    const auto startIndex = static_cast<std::int32_t>(grid.indexOf(start));
    const auto goalIndex = static_cast<std::int32_t>(grid.indexOf(goal));
    const float startHeuristic = grid.estimate(start, goal, heuristic);
    at(startIndex).score = 0.0F;
    push({.estimate = startHeuristic * options.weight, .heuristic = startHeuristic, .cell = startIndex});

    while (!open.empty()) {
        const OpenNode current = pop();
        Node& node = at(current.cell);
        if (node.closed) {
            continue;
        }
        node.closed = true;
        ++expanded;
        if (current.cell == goalIndex) {
            return;
        }

        const float score = node.score;
        // clang-format off
        grid.forEachStep(grid.cellAt(static_cast<std::size_t>(current.cell)), options.diagonal, [&](Grid::Cell next, float length) {
            const auto nextIndex = static_cast<std::int32_t>(grid.indexOf(next));
            Node& neighbor = at(nextIndex);
            const float tentative = score + length * grid.getCostAt(static_cast<std::size_t>(nextIndex));
            if (neighbor.closed || tentative >= neighbor.score) {
                return;
            }
            neighbor.score = tentative;
            neighbor.parent = current.cell;
            const float remaining = grid.estimate(next, goal, heuristic);
            push({.estimate = tentative + remaining * options.weight, .heuristic = remaining, .cell = nextIndex});
        });
        // clang-format on
    }
}

std::size_t GridSearch::prunedNeighbors(const Grid& grid, Grid::Cell point, std::optional<Grid::Cell> parent, bool diagonal, std::array<Grid::Cell, 8>& candidates) {
    const auto passable = [&grid](int x, int y) { return grid.isWalkable(grid.fromLattice({x, y})); };
    const int x = point.x;
    const int y = point.y;
    std::size_t count = 0;
    const auto add = [&](int nextX, int nextY) { candidates[count++] = {nextX, nextY}; };

    // The start has no direction yet, so every natural neighbor is a candidate.
    if (!parent) {
        grid.forEachStep(grid.fromLattice(point), diagonal, [&](Grid::Cell next, float) { candidates[count++] = grid.toLattice(next); });
        return count;
    }

    const int dx = (x > parent->x) - (x < parent->x);
    const int dy = (y > parent->y) - (y < parent->y);
    if (!diagonal) {
        const Grid::Cell sideA = dx != 0 ? Grid::Cell{x, y - 1} : Grid::Cell{x - 1, y};
        const Grid::Cell sideB = dx != 0 ? Grid::Cell{x, y + 1} : Grid::Cell{x + 1, y};
        for (const Grid::Cell next : {sideA, sideB, Grid::Cell{x + dx, y + dy}}) {
            if (passable(next.x, next.y)) {
                add(next.x, next.y);
            }
        }
        return count;
    }

    if (dx != 0 && dy != 0) {
        const bool alongY = passable(x, y + dy);
        const bool alongX = passable(x + dx, y);
        if (alongY) {
            add(x, y + dy);
        }
        if (alongX) {
            add(x + dx, y);
        }
        if (alongY && alongX) {
            add(x + dx, y + dy);
        }
        return count;
    }

    // A straight move keeps going and may also turn to either side, where diagonal turns need the cell ahead open.
    const Grid::Cell ahead{x + dx, y + dy};
    const Grid::Cell sideA = dx != 0 ? Grid::Cell{x, y + 1} : Grid::Cell{x + 1, y};
    const Grid::Cell sideB = dx != 0 ? Grid::Cell{x, y - 1} : Grid::Cell{x - 1, y};
    const bool aheadOpen = passable(ahead.x, ahead.y);
    const bool sideAOpen = passable(sideA.x, sideA.y);
    const bool sideBOpen = passable(sideB.x, sideB.y);
    if (aheadOpen) {
        add(ahead.x, ahead.y);
        if (sideAOpen) {
            add(sideA.x + dx, sideA.y + dy);
        }
        if (sideBOpen) {
            add(sideB.x + dx, sideB.y + dy);
        }
    }
    if (sideAOpen) {
        add(sideA.x, sideA.y);
    }
    if (sideBOpen) {
        add(sideB.x, sideB.y);
    }
    return count;
}

std::optional<Grid::Cell> GridSearch::jump(const Grid& grid, Grid::Cell point, Grid::Cell direction, Grid::Cell goal, bool diagonal) {
    const auto passable = [&grid](int x, int y) { return grid.isWalkable(grid.fromLattice({x, y})); };
    const int dx = direction.x;
    const int dy = direction.y;
    for (;;) {
        const int x = point.x;
        const int y = point.y;
        if (!passable(x, y)) {
            return std::nullopt;
        }
        if (point == goal) {
            return point;
        }

        // A point is a jump point when a wall beside the path forces a turn, or when a diagonal move can reach one by going straight.
        if (dx != 0 && dy != 0) {
            if (jump(grid, {x + dx, y}, {dx, 0}, goal, diagonal) || jump(grid, {x, y + dy}, {0, dy}, goal, diagonal)) {
                return point;
            }
            if (!passable(x + dx, y) || !passable(x, y + dy)) {
                return std::nullopt;
            }
        } else if (dx != 0) {
            if ((passable(x, y - 1) && !passable(x - dx, y - 1)) || (passable(x, y + 1) && !passable(x - dx, y + 1))) {
                return point;
            }
        } else {
            if ((passable(x - 1, y) && !passable(x - 1, y - dy)) || (passable(x + 1, y) && !passable(x + 1, y - dy))) {
                return point;
            }
            if (!diagonal && (jump(grid, {x + 1, y}, {1, 0}, goal, diagonal) || jump(grid, {x - 1, y}, {-1, 0}, goal, diagonal))) {
                return point;
            }
        }
        point = {x + dx, y + dy};
    }
}

void GridSearch::searchJumpPoints(const Grid& grid, Grid::Cell start, Grid::Cell goal, const Options& options, Grid::Heuristic heuristic) {
    const auto startIndex = static_cast<std::int32_t>(grid.indexOf(start));
    const auto goalIndex = static_cast<std::int32_t>(grid.indexOf(goal));
    const Grid::Cell goalPoint = grid.toLattice(goal);
    const float startHeuristic = grid.estimate(start, goal, heuristic);
    at(startIndex).score = 0.0F;
    push({.estimate = startHeuristic * options.weight, .heuristic = startHeuristic, .cell = startIndex});

    std::array<Grid::Cell, 8> candidates{};
    while (!open.empty()) {
        const OpenNode current = pop();
        Node& node = at(current.cell);
        if (node.closed) {
            continue;
        }
        node.closed = true;
        ++expanded;
        if (current.cell == goalIndex) {
            return;
        }

        const Grid::Cell point = grid.toLattice(grid.cellAt(static_cast<std::size_t>(current.cell)));
        const std::optional<Grid::Cell> parent = node.parent < 0 ? std::nullopt : std::optional{grid.toLattice(grid.cellAt(static_cast<std::size_t>(node.parent)))};
        const std::size_t count = prunedNeighbors(grid, point, parent, options.diagonal, candidates);
        for (std::size_t index = 0; index < count; ++index) {
            const Grid::Cell direction{candidates[index].x - point.x, candidates[index].y - point.y};
            const std::optional<Grid::Cell> jumpPoint = jump(grid, candidates[index], direction, goalPoint, options.diagonal);
            if (!jumpPoint) {
                continue;
            }

            // Jumps run straight or diagonally, so their length follows from the number of steps.
            const Grid::Cell target = grid.fromLattice(*jumpPoint);
            const auto targetIndex = static_cast<std::int32_t>(grid.indexOf(target));
            Node& neighbor = at(targetIndex);
            const auto steps = static_cast<float>(std::max(std::abs(jumpPoint->x - point.x), std::abs(jumpPoint->y - point.y)));
            const float tentative = node.score + steps * (direction.x != 0 && direction.y != 0 ? Grid::kDiagonalStep : 1.0F);
            if (neighbor.closed || tentative >= neighbor.score) {
                continue;
            }
            neighbor.score = tentative;
            neighbor.parent = current.cell;
            const float remaining = grid.estimate(target, goal, heuristic);
            push({.estimate = tentative + remaining * options.weight, .heuristic = remaining, .cell = targetIndex});
        }
    }
}

void GridSearch::buildPath(const Grid& grid, std::int32_t goal, bool jumpPoints) {
    std::vector<Grid::Cell>& chain = jumpPoints ? jumps : path;
    chain.clear();
    for (std::int32_t cell = goal; cell >= 0; cell = at(cell).parent) {
        chain.push_back(grid.cellAt(static_cast<std::size_t>(cell)));
    }
    std::ranges::reverse(chain);
    if (!jumpPoints) {
        return;
    }

    // Jump points join along straight or diagonal lines of the lattice, so the cells between them follow step by step.
    path.clear();
    for (std::size_t index = 0; index + 1 < jumps.size(); ++index) {
        Grid::Cell point = grid.toLattice(jumps[index]);
        const Grid::Cell end = grid.toLattice(jumps[index + 1]);
        const Grid::Cell step{(end.x > point.x) - (end.x < point.x), (end.y > point.y) - (end.y < point.y)};
        for (; point != end; point = {point.x + step.x, point.y + step.y}) {
            path.push_back(grid.fromLattice(point));
        }
    }
    path.push_back(jumps.back());
}

} // namespace haylen::navigation2d
