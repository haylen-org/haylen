#include "haylen/2d/navigation/GraphSearch.hpp"

#include <algorithm>

namespace haylen::navigation2d {

bool GraphSearch::isWorse(const OpenNode& lhs, const OpenNode& rhs) noexcept {
    return lhs.estimate != rhs.estimate ? lhs.estimate > rhs.estimate : lhs.slot > rhs.slot;
}

void GraphSearch::begin(std::size_t slots) {
    if (nodes.size() < slots) {
        nodes.resize(slots);
    }
    ++visit;
    if (visit == 0) {
        std::ranges::fill(nodes, Node{});
        visit = 1;
    }
    open.clear();
}

GraphSearch::Node& GraphSearch::at(std::uint32_t slot) {
    Node& node = nodes[slot];
    if (node.visit != visit) {
        node = {.visit = visit};
    }
    return node;
}

void GraphSearch::search(const Graph& graph, std::uint32_t start, std::int64_t goal, bool toGoal) {
    begin(graph.points.size());
    const math::Vec2 target = toGoal ? graph.getPosition(goal) : math::Vec2{};
    const std::uint32_t goalSlot = toGoal ? graph.slotOf(goal) : start;
    at(start).score = 0.0F;
    open.push_back({.estimate = toGoal ? math::Vec2::distance(graph.points[start].position, target) : 0.0F, .slot = start});

    while (!open.empty()) {
        std::ranges::pop_heap(open, &GraphSearch::isWorse);
        const OpenNode current = open.back();
        open.pop_back();
        Node& node = at(current.slot);
        if (node.closed) {
            continue;
        }
        node.closed = true;
        if (toGoal && current.slot == goalSlot) {
            return;
        }

        const Graph::Point& point = graph.points[current.slot];
        for (const std::uint32_t next : point.outgoing) {
            const Graph::Point& neighbor = graph.points[next];
            Node& state = at(next);
            if (!neighbor.enabled || state.closed) {
                continue;
            }
            const float score = node.score + math::Vec2::distance(point.position, neighbor.position) * neighbor.weight;
            if (score >= state.score) {
                continue;
            }
            state.score = score;
            state.parent = static_cast<std::int32_t>(current.slot);
            open.push_back({.estimate = score + (toGoal ? math::Vec2::distance(neighbor.position, target) : 0.0F), .slot = next});
            std::ranges::push_heap(open, &GraphSearch::isWorse);
        }
    }
}

std::span<const std::int64_t> GraphSearch::findPath(const Graph& graph, std::int64_t start, std::int64_t goal) {
    const std::uint32_t startSlot = graph.slotOf(start);
    const std::uint32_t goalSlot = graph.slotOf(goal);
    path.clear();
    cost = std::numeric_limits<float>::infinity();
    if (!graph.points[startSlot].enabled || !graph.points[goalSlot].enabled) {
        return {};
    }

    search(graph, startSlot, goal, true);
    const Node& end = at(goalSlot);
    if (!end.closed) {
        return {};
    }
    cost = end.score;
    for (std::int32_t slot = static_cast<std::int32_t>(goalSlot); slot >= 0; slot = at(static_cast<std::uint32_t>(slot)).parent) {
        path.push_back(graph.points[static_cast<std::size_t>(slot)].id);
    }
    std::ranges::reverse(path);
    return path;
}

void GraphSearch::computeDistances(const Graph& graph, std::int64_t source) {
    const std::uint32_t slot = graph.slotOf(source);
    path.clear();
    cost = std::numeric_limits<float>::infinity();
    if (graph.points[slot].enabled) {
        search(graph, slot, source, false);
        return;
    }
    begin(graph.points.size());
}

float GraphSearch::getDistance(const Graph& graph, std::int64_t id) const {
    const std::uint32_t slot = graph.slotOf(id);
    if (slot >= nodes.size() || nodes[slot].visit != visit) {
        return std::numeric_limits<float>::infinity();
    }
    return nodes[slot].score;
}

} // namespace haylen::navigation2d
