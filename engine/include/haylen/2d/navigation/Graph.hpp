#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include "haylen/math/Vec2.hpp"

namespace haylen::navigation2d {

class GraphSearch;

// A graph of waypoints for path finding. Callers pick the point ids. Each point has a position, a weight that scales the cost of stepping into it and an enabled flag, and connections join points in one or both directions. Stepping into a point costs the distance to it times its weight.
class Graph final {
  public:
    // Adds a point, or moves an existing one and changes its weight while keeping its connections. Weights below 1 are rejected so searches stay optimal.
    void addPoint(std::int64_t id, math::Vec2 position, float weight = 1.0F);
    bool removePoint(std::int64_t id);
    void clear() noexcept;

    [[nodiscard]] bool hasPoint(std::int64_t id) const;
    [[nodiscard]] std::size_t size() const noexcept {
        return slots.size();
    }

    [[nodiscard]] math::Vec2 getPosition(std::int64_t id) const;
    void setPosition(std::int64_t id, math::Vec2 value);
    [[nodiscard]] float getWeight(std::int64_t id) const;
    void setWeight(std::int64_t id, float value);

    // Disabled points stay in the graph with their connections, but no path passes through them.
    [[nodiscard]] bool isEnabled(std::int64_t id) const;
    void setEnabled(std::int64_t id, bool value);

    void connect(std::int64_t from, std::int64_t to, bool bidirectional = true);
    void disconnect(std::int64_t from, std::int64_t to, bool bidirectional = true);

    // Tells whether a path may step from the first point into the second.
    [[nodiscard]] bool isConnected(std::int64_t from, std::int64_t to) const;

    // Fills ids with the points reachable in one step from the point, in the order they were connected.
    void getNeighbors(std::int64_t id, std::vector<std::int64_t>& ids) const;

    // Fills ids with the id of every point in ascending order.
    void getPoints(std::vector<std::int64_t>& ids) const;

    // Returns the point closest to the position, where ties go to the smaller id, or nothing in an empty graph.
    [[nodiscard]] std::optional<std::int64_t> getClosestPoint(math::Vec2 position, bool includeDisabled = false) const;

  private:
    friend class GraphSearch;

    struct Point {
        std::int64_t id = 0;
        math::Vec2 position{};
        float weight = 1.0F;
        bool enabled = true;
        std::vector<std::uint32_t> outgoing;
        std::vector<std::uint32_t> incoming;
    };

    static void requireWeight(float value);
    [[nodiscard]] std::uint32_t slotOf(std::int64_t id) const;

    std::vector<Point> points;
    std::vector<std::uint32_t> freeSlots;
    std::unordered_map<std::int64_t, std::uint32_t> slots;
};

} // namespace haylen::navigation2d
