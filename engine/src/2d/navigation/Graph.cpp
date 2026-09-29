#include "haylen/2d/navigation/Graph.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace haylen::navigation2d {

void Graph::requireWeight(float value) {
    if (!(value >= 1.0F) || !std::isfinite(value)) {
        throw std::invalid_argument("A graph point weight must be finite and at least 1.");
    }
}

std::uint32_t Graph::slotOf(std::int64_t id) const {
    const auto slot = slots.find(id);
    if (slot == slots.end()) {
        throw std::out_of_range("Point " + std::to_string(id) + " is not in the graph.");
    }
    return slot->second;
}

void Graph::addPoint(std::int64_t id, math::Vec2 position, float weight) {
    requireWeight(weight);
    if (const auto existing = slots.find(id); existing != slots.end()) {
        Point& point = points[existing->second];
        point.position = position;
        point.weight = weight;
        return;
    }

    std::uint32_t slot = 0;
    if (freeSlots.empty()) {
        slot = static_cast<std::uint32_t>(points.size());
        points.emplace_back();
    } else {
        slot = freeSlots.back();
        freeSlots.pop_back();
    }
    Point& point = points[slot];
    point.id = id;
    point.position = position;
    point.weight = weight;
    point.enabled = true;
    slots.emplace(id, slot);
}

bool Graph::removePoint(std::int64_t id) {
    const auto found = slots.find(id);
    if (found == slots.end()) {
        return false;
    }

    const std::uint32_t slot = found->second;
    Point& point = points[slot];
    for (const std::uint32_t target : point.outgoing) {
        std::erase(points[target].incoming, slot);
    }
    for (const std::uint32_t source : point.incoming) {
        std::erase(points[source].outgoing, slot);
    }
    point.outgoing.clear();
    point.incoming.clear();
    slots.erase(found);
    freeSlots.push_back(slot);
    return true;
}

void Graph::clear() noexcept {
    points.clear();
    freeSlots.clear();
    slots.clear();
}

bool Graph::hasPoint(std::int64_t id) const {
    return slots.contains(id);
}

math::Vec2 Graph::getPosition(std::int64_t id) const {
    return points[slotOf(id)].position;
}

void Graph::setPosition(std::int64_t id, math::Vec2 value) {
    points[slotOf(id)].position = value;
}

float Graph::getWeight(std::int64_t id) const {
    return points[slotOf(id)].weight;
}

void Graph::setWeight(std::int64_t id, float value) {
    requireWeight(value);
    points[slotOf(id)].weight = value;
}

bool Graph::isEnabled(std::int64_t id) const {
    return points[slotOf(id)].enabled;
}

void Graph::setEnabled(std::int64_t id, bool value) {
    points[slotOf(id)].enabled = value;
}

void Graph::connect(std::int64_t from, std::int64_t to, bool bidirectional) {
    const std::uint32_t source = slotOf(from);
    const std::uint32_t target = slotOf(to);
    if (source == target) {
        throw std::invalid_argument("A graph point cannot connect to itself.");
    }
    if (std::ranges::find(points[source].outgoing, target) == points[source].outgoing.end()) {
        points[source].outgoing.push_back(target);
        points[target].incoming.push_back(source);
    }
    if (bidirectional) {
        connect(to, from, false);
    }
}

void Graph::disconnect(std::int64_t from, std::int64_t to, bool bidirectional) {
    const std::uint32_t source = slotOf(from);
    const std::uint32_t target = slotOf(to);
    std::erase(points[source].outgoing, target);
    std::erase(points[target].incoming, source);
    if (bidirectional) {
        disconnect(to, from, false);
    }
}

bool Graph::isConnected(std::int64_t from, std::int64_t to) const {
    const std::vector<std::uint32_t>& outgoing = points[slotOf(from)].outgoing;
    return std::ranges::find(outgoing, slotOf(to)) != outgoing.end();
}

void Graph::getNeighbors(std::int64_t id, std::vector<std::int64_t>& ids) const {
    ids.clear();
    for (const std::uint32_t target : points[slotOf(id)].outgoing) {
        ids.push_back(points[target].id);
    }
}

void Graph::getPoints(std::vector<std::int64_t>& ids) const {
    ids.clear();
    for (const auto& [id, slot] : slots) {
        ids.push_back(id);
    }
    std::ranges::sort(ids);
}

std::optional<std::int64_t> Graph::getClosestPoint(math::Vec2 position, bool includeDisabled) const {
    std::optional<std::int64_t> closest;
    float closestDistance = std::numeric_limits<float>::infinity();
    for (const auto& [id, slot] : slots) {
        const Point& point = points[slot];
        if (!point.enabled && !includeDisabled) {
            continue;
        }
        const float distance = math::Vec2::distanceSquared(point.position, position);
        if (distance < closestDistance || (distance == closestDistance && id < *closest)) {
            closestDistance = distance;
            closest = id;
        }
    }
    return closest;
}

} // namespace haylen::navigation2d
