#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

#include "haylen/2d/navigation/Grid.hpp"

namespace haylen::navigation2d {

// Finds paths on large square navigation grids in two levels, the HPA* of Botea, Müller and Schaeffer. The grid splits into square clusters joined by entrances on their shared sides, which form a small abstract graph that is searched first, and then only the cells along the chosen route are searched. Paths are near optimal, usually within a few percent of the cheapest one. Every call takes the grid it was built from, and after cells change, `update` rebuilds only the clusters around them.
class HierarchicalPathfinder final {
  public:
    struct Options {
        // A cluster size above the size of the grid makes one cluster that covers it.
        int clusterSize = 16;
        bool diagonal = true;
    };

    explicit HierarchicalPathfinder(const Grid& grid, const Options& value = kDefaultOptions);

    // Throws `std::invalid_argument` when the grid is not square or the clusters are smaller than 2 cells, which lets background builds reject bad arguments before they start.
    static void requireValid(const Grid& grid, const Options& value);

    // Rebuilds the clusters that hold the cells from first to last, both included, after their walkability or costs changed.
    void update(const Grid& grid, Grid::Cell first, Grid::Cell last);
    void rebuild(const Grid& grid);

    // Returns the cells from start to goal, both included, or an empty path when either end is blocked, the goal is unreachable or the route crosses cells that changed without an update. The path stays valid until the next search.
    std::span<const Grid::Cell> findPath(const Grid& grid, Grid::Cell start, Grid::Cell goal);

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
    void requireGrid(const Grid& grid) const;

    [[nodiscard]] int clusterOf(Grid::Cell cell) const noexcept;
    [[nodiscard]] Bounds boundsOf(int cluster) const noexcept;

    // Rebuilds the entrances on every side of the given clusters and the paths inside them and inside their neighbors.
    void rebuildClusters(const Grid& grid, std::span<const int> dirty);
    void removeBorder(std::vector<std::int32_t>& border);
    void createBorder(const Grid& grid, int first, int second, bool horizontal, std::vector<std::int32_t>& border);
    [[nodiscard]] std::int32_t addNode(const Grid& grid, Grid::Cell cell, int cluster);
    void connectInside(const Grid& grid, int cluster);

    // Searches the cells of one cluster from the source, forward along steps or backward toward the source, until it settles the target or, without one, every cell. Returns `true` when it reached the target.
    bool searchLocal(const Grid& grid, const Bounds& bounds, Grid::Cell source, bool backward, std::int32_t target);
    [[nodiscard]] float localScore(const Bounds& bounds, Grid::Cell cell) const noexcept;
    [[nodiscard]] std::int32_t localIndex(const Bounds& bounds, Grid::Cell cell) const noexcept;

    // Appends the local path from the source of the last forward search to the cell, skipping a first cell equal to the end of the path.
    void appendLocalPath(const Bounds& bounds, Grid::Cell cell);
    void appendCell(Grid::Cell cell);

    // Turns the route of entrances into cells, and returns `false` when a cell of the route or a path between its entrances no longer exists.
    bool refineRoute(const Grid& grid, Grid::Cell start, Grid::Cell goal);

    Options options;
    int width = 0;
    int height = 0;
    int clustersX = 0;
    int clustersY = 0;

    std::vector<Node> nodes;
    std::vector<std::int32_t> freeNodes;
    std::vector<std::vector<std::int32_t>> clusterNodes;
    std::vector<std::vector<std::int32_t>> horizontalBorders;
    std::vector<std::vector<std::int32_t>> verticalBorders;

    // The local search buffers cover one cluster, clipped to the grid, row by row.
    std::vector<LocalNode> local;
    int localColumns = 0;
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
