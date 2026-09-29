#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <span>
#include <vector>

#include "haylen/2d/navigation/Grid.hpp"
#include "haylen/core/JobSystem.hpp"

namespace haylen::navigation2d {

// Finds paths on large square navigation grids in two levels, the HPA* of Botea, Müller and Schaeffer. The grid splits into square clusters joined by entrances on their shared sides, which form a small abstract graph that is searched first, and then only the cells along the chosen route are searched. Paths are near optimal, usually within a few percent of the cheapest one. It keeps a reference to its grid, which must outlive it, and after cells change, update rebuilds only the clusters around them.
class HierarchicalPathfinder final {
  public:
    struct Options {
        int clusterSize = 16;
        bool diagonal = true;
    };

    using BuildCompletion = std::function<void(core::JobSystem::Result<HierarchicalPathfinder> result)>;

    explicit HierarchicalPathfinder(const Grid& navigationGrid, const Options& value = kDefaultOptions);

    // Builds the path finder of the grid from a copy of its cells on the task pool, which keeps large grids from stalling a frame, and calls completion on the frame thread with a path finder that reads the grid itself, or with the error. The grid must outlive the build, and cells that change before the completion need update.
    static void buildAsync(core::JobSystem& jobs, const Grid& navigationGrid, const Options& value, BuildCompletion completion);

    // Rebuilds the clusters that hold the cells from first to last, both included, after their walkability or costs changed.
    void update(Grid::Cell first, Grid::Cell last);
    void rebuild();

    // Returns the cells from start to goal, both included, or an empty path when either end is blocked or the goal is unreachable. The path stays valid until the next search.
    std::span<const Grid::Cell> findPath(Grid::Cell start, Grid::Cell goal);

    // Returns the cost of the last path found, which is infinite when there was none.
    [[nodiscard]] float getCost() const noexcept {
        return cost;
    }

    // Returns the number of entrance cells in the abstract graph.
    [[nodiscard]] std::size_t getNodeCount() const noexcept {
        return nodes.size() - freeNodes.size();
    }
    [[nodiscard]] const Options& getOptions() const noexcept {
        return options;
    }

  private:
    struct Edge {
        std::int32_t target = 0;
        float cost = 0.0F;
        bool inter = false;
    };

    struct Node {
        std::int32_t cell = 0;
        std::int32_t cluster = 0;
        std::vector<Edge> edges;
    };

    struct Bounds {
        int left = 0;
        int top = 0;
        int right = 0;
        int bottom = 0;

        [[nodiscard]] bool contains(Grid::Cell cell) const noexcept {
            return cell.x >= left && cell.y >= top && cell.x < right && cell.y < bottom;
        }
    };

    struct OpenNode {
        float estimate = 0.0F;
        std::int32_t index = 0;
    };

    struct LocalNode {
        float score = std::numeric_limits<float>::infinity();
        std::int32_t parent = -1;
        std::uint32_t visit = 0;
    };

    static const Options kDefaultOptions;

    // Entrances shorter than this get one crossing in their middle, and longer ones get a crossing at each end.
    static constexpr int kLongEntrance = 6;

    [[nodiscard]] static bool isWorse(const OpenNode& lhs, const OpenNode& rhs) noexcept;

    [[nodiscard]] int clusterOf(Grid::Cell cell) const noexcept;
    [[nodiscard]] Bounds boundsOf(int cluster) const noexcept;

    // Rebuilds the entrances on every side of the given clusters and the paths inside them and inside their neighbors.
    void rebuildClusters(std::span<const int> dirty);
    void removeBorder(std::vector<std::int32_t>& border);
    void createBorder(int first, int second, bool horizontal, std::vector<std::int32_t>& border);
    [[nodiscard]] std::int32_t addNode(Grid::Cell cell, int cluster);
    void connectInside(int cluster);

    // Searches the cells of one cluster from the source, forward along steps or backward toward the source, until it settles the target or, without one, every cell. Returns true when it reached the target.
    bool searchLocal(const Bounds& bounds, Grid::Cell source, bool backward, std::int32_t target);
    [[nodiscard]] float localScore(const Bounds& bounds, Grid::Cell cell) const noexcept;
    [[nodiscard]] std::int32_t localIndex(const Bounds& bounds, Grid::Cell cell) const noexcept;

    // Appends the local path from the source of the last forward search to the cell, skipping a first cell equal to the end of the path.
    void appendLocalPath(const Bounds& bounds, Grid::Cell cell);
    void appendCell(Grid::Cell cell);

    const Grid* grid;
    Options options;
    int clustersX = 0;
    int clustersY = 0;

    std::vector<Node> nodes;
    std::vector<std::int32_t> freeNodes;
    std::vector<std::vector<std::int32_t>> clusterNodes;
    std::vector<std::vector<std::int32_t>> horizontalBorders;
    std::vector<std::vector<std::int32_t>> verticalBorders;

    std::vector<LocalNode> local;
    std::uint32_t localVisit = 0;
    std::vector<OpenNode> open;

    std::vector<float> abstractScores;
    std::vector<std::int32_t> abstractParents;
    std::vector<std::uint8_t> abstractClosed;
    std::vector<float> goalCosts;
    std::vector<std::int32_t> route;
    std::vector<Grid::Cell> path;
    float cost = std::numeric_limits<float>::infinity();
};

} // namespace haylen::navigation2d
