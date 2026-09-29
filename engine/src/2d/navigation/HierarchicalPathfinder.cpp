#include "haylen/2d/navigation/HierarchicalPathfinder.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <utility>

namespace haylen::navigation2d {

const HierarchicalPathfinder::Options HierarchicalPathfinder::kDefaultOptions{};

HierarchicalPathfinder::HierarchicalPathfinder(const Grid& navigationGrid, const Options& value) : grid(&navigationGrid), options(value) {
    if (grid->getLayout().topology != Grid::Topology::Square) {
        throw std::invalid_argument("Hierarchical path finding needs a square grid->");
    }
    if (value.clusterSize < 2) {
        throw std::invalid_argument("Hierarchical path finding needs clusters of at least 2 cells.");
    }

    const int size = value.clusterSize;
    clustersX = (grid->getWidth() + size - 1) / size;
    clustersY = (grid->getHeight() + size - 1) / size;
    clusterNodes.resize(static_cast<std::size_t>(clustersX) * static_cast<std::size_t>(clustersY));
    horizontalBorders.resize(static_cast<std::size_t>(clustersX - 1) * static_cast<std::size_t>(clustersY));
    verticalBorders.resize(static_cast<std::size_t>(clustersX) * static_cast<std::size_t>(clustersY - 1));
    local.resize(static_cast<std::size_t>(size) * static_cast<std::size_t>(size));
    rebuild();
}

void HierarchicalPathfinder::buildAsync(core::JobSystem& jobs, const Grid& navigationGrid, const Options& value, BuildCompletion completion) {
    // clang-format off
    jobs.run([copy = navigationGrid, value] {
        return HierarchicalPathfinder(copy, value);
    }, [target = &navigationGrid, completion = std::move(completion)](core::JobSystem::Result<HierarchicalPathfinder> result) {
        // The job built the path finder on a copy with the same cells, so it reads the grid itself from here on.
        if (result.value) {
            result.value->grid = target;
        }
        completion(std::move(result));
    });
    // clang-format on
}

bool HierarchicalPathfinder::isWorse(const OpenNode& lhs, const OpenNode& rhs) noexcept {
    return lhs.estimate != rhs.estimate ? lhs.estimate > rhs.estimate : lhs.index > rhs.index;
}

int HierarchicalPathfinder::clusterOf(Grid::Cell cell) const noexcept {
    return (cell.y / options.clusterSize) * clustersX + cell.x / options.clusterSize;
}

HierarchicalPathfinder::Bounds HierarchicalPathfinder::boundsOf(int cluster) const noexcept {
    const int size = options.clusterSize;
    const int left = (cluster % clustersX) * size;
    const int top = (cluster / clustersX) * size;
    return {.left = left, .top = top, .right = std::min(left + size, grid->getWidth()), .bottom = std::min(top + size, grid->getHeight())};
}

void HierarchicalPathfinder::rebuild() {
    std::vector<int> all(clusterNodes.size());
    std::iota(all.begin(), all.end(), 0);
    rebuildClusters(all);
}

void HierarchicalPathfinder::update(Grid::Cell first, Grid::Cell last) {
    const int left = std::clamp(std::min(first.x, last.x), 0, grid->getWidth() - 1) / options.clusterSize;
    const int right = std::clamp(std::max(first.x, last.x), 0, grid->getWidth() - 1) / options.clusterSize;
    const int top = std::clamp(std::min(first.y, last.y), 0, grid->getHeight() - 1) / options.clusterSize;
    const int bottom = std::clamp(std::max(first.y, last.y), 0, grid->getHeight() - 1) / options.clusterSize;
    std::vector<int> dirty;
    for (int y = top; y <= bottom; ++y) {
        for (int x = left; x <= right; ++x) {
            dirty.push_back(y * clustersX + x);
        }
    }
    rebuildClusters(dirty);
}

std::int32_t HierarchicalPathfinder::addNode(Grid::Cell cell, int cluster) {
    std::int32_t id = 0;
    if (freeNodes.empty()) {
        id = static_cast<std::int32_t>(nodes.size());
        nodes.emplace_back();
    } else {
        id = freeNodes.back();
        freeNodes.pop_back();
    }
    Node& node = nodes[static_cast<std::size_t>(id)];
    node.cell = static_cast<std::int32_t>(grid->indexOf(cell));
    node.cluster = cluster;
    node.edges.clear();
    clusterNodes[static_cast<std::size_t>(cluster)].push_back(id);
    return id;
}

void HierarchicalPathfinder::removeBorder(std::vector<std::int32_t>& border) {
    for (const std::int32_t id : border) {
        Node& node = nodes[static_cast<std::size_t>(id)];
        std::erase(clusterNodes[static_cast<std::size_t>(node.cluster)], id);
        node.edges.clear();
        freeNodes.push_back(id);
    }
    border.clear();
}

void HierarchicalPathfinder::createBorder(int first, int second, bool horizontal, std::vector<std::int32_t>& border) {
    const Bounds bounds = boundsOf(first);
    const int start = horizontal ? bounds.top : bounds.left;
    const int end = horizontal ? bounds.bottom : bounds.right;
    const auto pairAt = [&](int along, bool secondSide) { return horizontal ? Grid::Cell{bounds.right - (secondSide ? 0 : 1), along} : Grid::Cell{along, bounds.bottom - (secondSide ? 0 : 1)}; };

    // Each run of cells open on both sides of the border becomes an entrance, crossed in its middle when short and at both ends when long.
    // clang-format off
    const auto addEntrance = [&](int from, int to) {
        const int length = to - from + 1;
        const std::array<int, 2> crossings{length < kLongEntrance ? from + length / 2 : from, to};
        for (std::size_t crossing = 0; crossing < (length < kLongEntrance ? 1U : 2U); ++crossing) {
            const int along = crossings[crossing];
            const Grid::Cell near = pairAt(along, false);
            const Grid::Cell far = pairAt(along, true);
            const std::int32_t nearNode = addNode(near, first);
            const std::int32_t farNode = addNode(far, second);
            nodes[static_cast<std::size_t>(nearNode)].edges.push_back({.target = farNode, .cost = grid->getCostAt(grid->indexOf(far)), .inter = true});
            nodes[static_cast<std::size_t>(farNode)].edges.push_back({.target = nearNode, .cost = grid->getCostAt(grid->indexOf(near)), .inter = true});
            border.push_back(nearNode);
            border.push_back(farNode);
        }
    };
    // clang-format on

    int runStart = -1;
    for (int along = start; along <= end; ++along) {
        const bool passable = along < end && grid->isWalkable(pairAt(along, false)) && grid->isWalkable(pairAt(along, true));
        if (passable && runStart < 0) {
            runStart = along;
        } else if (!passable && runStart >= 0) {
            addEntrance(runStart, along - 1);
            runStart = -1;
        }
    }
}

void HierarchicalPathfinder::connectInside(int cluster) {
    const Bounds bounds = boundsOf(cluster);
    const std::vector<std::int32_t>& members = clusterNodes[static_cast<std::size_t>(cluster)];
    for (const std::int32_t member : members) {
        std::erase_if(nodes[static_cast<std::size_t>(member)].edges, [](const Edge& edge) { return !edge.inter; });
    }

    for (const std::int32_t from : members) {
        searchLocal(bounds, grid->cellAt(static_cast<std::size_t>(nodes[static_cast<std::size_t>(from)].cell)), false, -1);
        for (const std::int32_t to : members) {
            const float score = localScore(bounds, grid->cellAt(static_cast<std::size_t>(nodes[static_cast<std::size_t>(to)].cell)));
            if (to != from && std::isfinite(score)) {
                nodes[static_cast<std::size_t>(from)].edges.push_back({.target = to, .cost = score});
            }
        }
    }
}

void HierarchicalPathfinder::rebuildClusters(std::span<const int> dirty) {
    std::vector<std::uint8_t> horizontal(horizontalBorders.size(), 0);
    std::vector<std::uint8_t> vertical(verticalBorders.size(), 0);
    std::vector<std::uint8_t> reconnect(clusterNodes.size(), 0);

    // A cluster owns the entrances on its four sides, which are shared with its neighbors, so the neighbors reconnect their paths too.
    for (const int cluster : dirty) {
        const int x = cluster % clustersX;
        const int y = cluster / clustersX;
        reconnect[static_cast<std::size_t>(cluster)] = 1;
        if (x + 1 < clustersX) {
            horizontal[static_cast<std::size_t>(y * (clustersX - 1) + x)] = 1;
            reconnect[static_cast<std::size_t>(cluster + 1)] = 1;
        }
        if (x > 0) {
            horizontal[static_cast<std::size_t>(y * (clustersX - 1) + x - 1)] = 1;
            reconnect[static_cast<std::size_t>(cluster - 1)] = 1;
        }
        if (y + 1 < clustersY) {
            vertical[static_cast<std::size_t>(y * clustersX + x)] = 1;
            reconnect[static_cast<std::size_t>(cluster + clustersX)] = 1;
        }
        if (y > 0) {
            vertical[static_cast<std::size_t>((y - 1) * clustersX + x)] = 1;
            reconnect[static_cast<std::size_t>(cluster - clustersX)] = 1;
        }
    }

    for (std::size_t border = 0; border < horizontal.size(); ++border) {
        if (horizontal[border] != 0) {
            removeBorder(horizontalBorders[border]);
        }
    }
    for (std::size_t border = 0; border < vertical.size(); ++border) {
        if (vertical[border] != 0) {
            removeBorder(verticalBorders[border]);
        }
    }
    for (std::size_t border = 0; border < horizontal.size(); ++border) {
        if (horizontal[border] != 0) {
            const int cluster = static_cast<int>(border) / (clustersX - 1) * clustersX + static_cast<int>(border) % (clustersX - 1);
            createBorder(cluster, cluster + 1, true, horizontalBorders[border]);
        }
    }
    for (std::size_t border = 0; border < vertical.size(); ++border) {
        if (vertical[border] != 0) {
            const auto cluster = static_cast<int>(border);
            createBorder(cluster, cluster + clustersX, false, verticalBorders[border]);
        }
    }
    for (std::size_t cluster = 0; cluster < reconnect.size(); ++cluster) {
        if (reconnect[cluster] != 0) {
            connectInside(static_cast<int>(cluster));
        }
    }
}

std::int32_t HierarchicalPathfinder::localIndex(const Bounds& bounds, Grid::Cell cell) const noexcept {
    return (cell.y - bounds.top) * options.clusterSize + (cell.x - bounds.left);
}

float HierarchicalPathfinder::localScore(const Bounds& bounds, Grid::Cell cell) const noexcept {
    const LocalNode& node = local[static_cast<std::size_t>(localIndex(bounds, cell))];
    return node.visit == localVisit ? node.score : std::numeric_limits<float>::infinity();
}

bool HierarchicalPathfinder::searchLocal(const Bounds& bounds, Grid::Cell source, bool backward, std::int32_t target) {
    ++localVisit;
    if (localVisit == 0) {
        std::ranges::fill(local, LocalNode{});
        localVisit = 1;
    }
    // clang-format off
    const auto stateOf = [this](std::int32_t index) -> LocalNode& {
        LocalNode& node = local[static_cast<std::size_t>(index)];
        if (node.visit != localVisit) {
            node = {.visit = localVisit};
        }
        return node;
    };
    // clang-format on

    open.clear();
    const std::int32_t sourceIndex = localIndex(bounds, source);
    stateOf(sourceIndex).score = 0.0F;
    open.push_back({.estimate = 0.0F, .index = sourceIndex});
    while (!open.empty()) {
        std::ranges::pop_heap(open, &HierarchicalPathfinder::isWorse);
        const OpenNode current = open.back();
        open.pop_back();
        const float score = stateOf(current.index).score;
        if (current.estimate > score) {
            continue;
        }
        if (current.index == target) {
            return true;
        }

        // Walking backward toward the source steps into the current cell, and walking forward steps into the neighbor.
        const Grid::Cell cell{bounds.left + current.index % options.clusterSize, bounds.top + current.index / options.clusterSize};
        const float entry = grid->getCostAt(grid->indexOf(cell));
        // clang-format off
        grid->forEachStep(cell, options.diagonal, [&](Grid::Cell next, float length) {
            if (!bounds.contains(next)) {
                return;
            }
            const std::int32_t nextIndex = localIndex(bounds, next);
            LocalNode& state = stateOf(nextIndex);
            const float candidate = score + length * (backward ? entry : grid->getCostAt(grid->indexOf(next)));
            if (candidate < state.score) {
                state.score = candidate;
                state.parent = current.index;
                open.push_back({.estimate = candidate, .index = nextIndex});
                std::ranges::push_heap(open, &HierarchicalPathfinder::isWorse);
            }
        });
        // clang-format on
    }
    return false;
}

void HierarchicalPathfinder::appendCell(Grid::Cell cell) {
    if (path.empty() || path.back() != cell) {
        path.push_back(cell);
    }
}

void HierarchicalPathfinder::appendLocalPath(const Bounds& bounds, Grid::Cell cell) {
    const std::size_t begin = path.size();
    for (std::int32_t index = localIndex(bounds, cell); index >= 0; index = local[static_cast<std::size_t>(index)].parent) {
        path.push_back({bounds.left + index % options.clusterSize, bounds.top + index / options.clusterSize});
    }
    std::reverse(path.begin() + static_cast<std::ptrdiff_t>(begin), path.end());
    if (begin > 0 && path[begin] == path[begin - 1]) {
        path.erase(path.begin() + static_cast<std::ptrdiff_t>(begin));
    }
}

std::span<const Grid::Cell> HierarchicalPathfinder::findPath(Grid::Cell start, Grid::Cell goal) {
    path.clear();
    cost = std::numeric_limits<float>::infinity();
    if (!grid->isWalkable(start) || !grid->isWalkable(goal)) {
        return {};
    }
    if (start == goal) {
        path.push_back(start);
        cost = 0.0F;
        return path;
    }

    const int startCluster = clusterOf(start);
    const int goalCluster = clusterOf(goal);
    const Bounds startBounds = boundsOf(startCluster);
    const Bounds goalBounds = boundsOf(goalCluster);

    // A path inside one cluster may never need to leave it, so the local path competes with the abstract one.
    float localCost = std::numeric_limits<float>::infinity();
    if (startCluster == goalCluster && searchLocal(startBounds, start, false, localIndex(startBounds, goal))) {
        localCost = localScore(startBounds, goal);
    }

    // The goal joins the abstract graph as one more node, reached from every entrance of its cluster.
    const std::size_t goalNode = nodes.size();
    goalCosts.assign(nodes.size(), std::numeric_limits<float>::infinity());
    searchLocal(goalBounds, goal, true, -1);
    for (const std::int32_t id : clusterNodes[static_cast<std::size_t>(goalCluster)]) {
        goalCosts[static_cast<std::size_t>(id)] = localScore(goalBounds, grid->cellAt(static_cast<std::size_t>(nodes[static_cast<std::size_t>(id)].cell)));
    }

    // The start seeds the search with the cost of reaching every entrance of its cluster.
    abstractScores.assign(nodes.size() + 1, std::numeric_limits<float>::infinity());
    abstractParents.assign(nodes.size() + 1, -1);
    abstractClosed.assign(nodes.size() + 1, 0);
    const Grid::Heuristic heuristic = options.diagonal ? Grid::Heuristic::Octile : Grid::Heuristic::Manhattan;
    searchLocal(startBounds, start, false, -1);
    for (const std::int32_t id : clusterNodes[static_cast<std::size_t>(startCluster)]) {
        const Grid::Cell cell = grid->cellAt(static_cast<std::size_t>(nodes[static_cast<std::size_t>(id)].cell));
        const float score = localScore(startBounds, cell);
        if (std::isfinite(score)) {
            abstractScores[static_cast<std::size_t>(id)] = score;
            open.push_back({.estimate = score + grid->estimate(cell, goal, heuristic), .index = id});
        }
    }
    std::ranges::make_heap(open, &HierarchicalPathfinder::isWorse);

    // clang-format off
    const auto relax = [&](std::int32_t target, float score, std::int32_t parent, float remaining) {
        if (abstractClosed[static_cast<std::size_t>(target)] != 0 || score >= abstractScores[static_cast<std::size_t>(target)]) {
            return;
        }
        abstractScores[static_cast<std::size_t>(target)] = score;
        abstractParents[static_cast<std::size_t>(target)] = parent;
        open.push_back({.estimate = score + remaining, .index = target});
        std::ranges::push_heap(open, &HierarchicalPathfinder::isWorse);
    };
    // clang-format on
    while (!open.empty()) {
        std::ranges::pop_heap(open, &HierarchicalPathfinder::isWorse);
        const OpenNode current = open.back();
        open.pop_back();
        const auto index = static_cast<std::size_t>(current.index);
        if (abstractClosed[index] != 0) {
            continue;
        }
        abstractClosed[index] = 1;
        if (index == goalNode) {
            break;
        }

        const float score = abstractScores[index];
        if (std::isfinite(goalCosts[index])) {
            relax(static_cast<std::int32_t>(goalNode), score + goalCosts[index], current.index, 0.0F);
        }
        for (const Edge& edge : nodes[index].edges) {
            const Grid::Cell cell = grid->cellAt(static_cast<std::size_t>(nodes[static_cast<std::size_t>(edge.target)].cell));
            relax(edge.target, score + edge.cost, current.index, grid->estimate(cell, goal, heuristic));
        }
    }

    const float abstractCost = abstractScores[goalNode];
    if (!std::isfinite(abstractCost) && !std::isfinite(localCost)) {
        return {};
    }
    if (localCost <= abstractCost) {
        searchLocal(startBounds, start, false, localIndex(startBounds, goal));
        appendLocalPath(startBounds, goal);
        cost = localCost;
        return path;
    }

    // Refines the route entrance by entrance: crossings between clusters are single steps, and paths inside a cluster come from a local search.
    route.clear();
    for (std::int32_t node = abstractParents[goalNode]; node >= 0; node = abstractParents[static_cast<std::size_t>(node)]) {
        route.push_back(node);
    }
    std::ranges::reverse(route);
    const auto cellOf = [this](std::int32_t node) { return grid->cellAt(static_cast<std::size_t>(nodes[static_cast<std::size_t>(node)].cell)); };

    searchLocal(startBounds, start, false, localIndex(startBounds, cellOf(route.front())));
    appendLocalPath(startBounds, cellOf(route.front()));
    for (std::size_t step = 1; step < route.size(); ++step) {
        const Node& from = nodes[static_cast<std::size_t>(route[step - 1])];
        const Node& to = nodes[static_cast<std::size_t>(route[step])];
        if (from.cluster != to.cluster) {
            appendCell(cellOf(route[step]));
            continue;
        }
        const Bounds bounds = boundsOf(from.cluster);
        searchLocal(bounds, cellOf(route[step - 1]), false, localIndex(bounds, cellOf(route[step])));
        appendLocalPath(bounds, cellOf(route[step]));
    }
    searchLocal(goalBounds, cellOf(route.back()), false, localIndex(goalBounds, goal));
    appendLocalPath(goalBounds, goal);
    cost = abstractCost;
    return path;
}

} // namespace haylen::navigation2d
