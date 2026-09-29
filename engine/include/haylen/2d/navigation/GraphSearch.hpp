#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

#include "haylen/2d/navigation/Graph.hpp"

namespace haylen::navigation2d {

// Finds paths through a waypoint graph with A* and measures the cost from one point to all others with Dijkstra, stepping around disabled points. It keeps its buffers between searches.
class GraphSearch final {
  public:
    // Returns the point ids from start to goal, both included, or an empty path when either end is disabled or the goal is unreachable. The path stays valid until the next search.
    std::span<const std::int64_t> findPath(const Graph& graph, std::int64_t start, std::int64_t goal);

    // Returns the cost of the last path found, which is infinite when there was none.
    [[nodiscard]] float getCost() const noexcept {
        return cost;
    }

    // Measures the cost of the cheapest path from the source to every point, which getDistance reads until the next search.
    void computeDistances(const Graph& graph, std::int64_t source);

    // Returns the cost from the last source to the point, which is infinite when the point is unreachable or disabled.
    [[nodiscard]] float getDistance(const Graph& graph, std::int64_t id) const;

  private:
    struct OpenNode {
        float estimate = 0.0F;
        std::uint32_t slot = 0;
    };

    struct Node {
        float score = std::numeric_limits<float>::infinity();
        std::int32_t parent = -1;
        std::uint32_t visit = 0;
        bool closed = false;
    };

    [[nodiscard]] static bool isWorse(const OpenNode& lhs, const OpenNode& rhs) noexcept;
    void begin(std::size_t slots);
    [[nodiscard]] Node& at(std::uint32_t slot);

    // Runs A* toward the goal slot, or Dijkstra over the whole graph when there is no goal.
    void search(const Graph& graph, std::uint32_t start, std::int64_t goal, bool toGoal);

    std::vector<Node> nodes;
    std::uint32_t visit = 0;
    std::vector<OpenNode> open;
    std::vector<std::int64_t> path;
    float cost = std::numeric_limits<float>::infinity();
};

} // namespace haylen::navigation2d
