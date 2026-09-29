#include "haylen/2d/navigation/NavMesh.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string>

#include "2d/navigation/ConstrainedTriangulation.hpp"
#include "haylen/math/Geometry.hpp"
#include "haylen/math/Rect.hpp"

namespace haylen::navigation2d {

bool NavMesh::isWorse(const OpenNode& lhs, const OpenNode& rhs) noexcept {
    return lhs.estimate != rhs.estimate ? lhs.estimate > rhs.estimate : lhs.triangle > rhs.triangle;
}

float NavMesh::area(math::Vec2 a, math::Vec2 b, math::Vec2 c) noexcept {
    return math::Vec2::cross(c - a, b - a);
}

void NavMesh::requirePolygon(std::span<const math::Vec2> polygon) {
    if (polygon.size() < 3) {
        throw std::invalid_argument("A navigation mesh polygon needs at least three points.");
    }
    for (const math::Vec2 point : polygon) {
        if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
            throw std::invalid_argument("A navigation mesh polygon needs finite points.");
        }
    }
}

void NavMesh::setBoundary(std::span<const math::Vec2> polygon) {
    requirePolygon(polygon);
    boundary.assign(polygon.begin(), polygon.end());
    dirty = true;
}

std::uint32_t NavMesh::addObstacle(std::span<const math::Vec2> polygon) {
    requirePolygon(polygon);
    const std::uint32_t id = nextObstacle++;
    obstacles.emplace(id, std::vector<math::Vec2>(polygon.begin(), polygon.end()));
    dirty = true;
    return id;
}

void NavMesh::setObstacle(std::uint32_t id, std::span<const math::Vec2> polygon) {
    requirePolygon(polygon);
    const auto obstacle = obstacles.find(id);
    if (obstacle == obstacles.end()) {
        throw std::out_of_range("Obstacle " + std::to_string(id) + " is not in the navigation mesh.");
    }
    obstacle->second.assign(polygon.begin(), polygon.end());
    dirty = true;
}

bool NavMesh::removeObstacle(std::uint32_t id) {
    if (obstacles.erase(id) == 0) {
        return false;
    }
    dirty = true;
    return true;
}

void NavMesh::clearObstacles() noexcept {
    dirty = dirty || !obstacles.empty();
    obstacles.clear();
}

void NavMesh::build() {
    if (boundary.empty()) {
        throw std::logic_error("A navigation mesh needs a boundary before it builds.");
    }

    segments.clear();
    // clang-format off
    const auto addOutline = [this](const std::vector<math::Vec2>& polygon) {
        for (std::size_t index = 0; index < polygon.size(); ++index) {
            segments.push_back({polygon[index], polygon[(index + 1) % polygon.size()]});
        }
    };
    // clang-format on
    addOutline(boundary);
    for (const auto& [id, polygon] : obstacles) {
        addOutline(polygon);
    }
    ConstrainedTriangulation triangulation;
    triangulation.build(segments);
    vertices = triangulation.getVertices();

    // A triangle is walkable when its centroid lies inside the boundary and outside every obstacle, which also merges overlapping obstacles.
    const std::vector<ConstrainedTriangulation::Triangle>& all = triangulation.getTriangles();
    std::vector<std::int32_t> remap(all.size(), -1);
    triangles.clear();
    for (std::size_t triangle = 0; triangle < all.size(); ++triangle) {
        const std::array<std::int32_t, 3>& corners = all[triangle].vertices;
        const math::Vec2 centroid = (vertices[static_cast<std::size_t>(corners[0])] + vertices[static_cast<std::size_t>(corners[1])] + vertices[static_cast<std::size_t>(corners[2])]) / 3.0F;
        const bool blocked = std::ranges::any_of(obstacles, [centroid](const auto& obstacle) { return math::Geometry::contains(obstacle.second, centroid); });
        if (!math::Geometry::contains(boundary, centroid) || blocked) {
            continue;
        }
        remap[triangle] = static_cast<std::int32_t>(triangles.size());
        triangles.push_back({.vertices = {static_cast<std::uint32_t>(corners[0]), static_cast<std::uint32_t>(corners[1]), static_cast<std::uint32_t>(corners[2])}, .neighbors = all[triangle].neighbors});
    }

    walls.clear();
    triangleIndex.clear();
    for (std::size_t triangle = 0; triangle < triangles.size(); ++triangle) {
        Triangle& current = triangles[triangle];
        for (std::size_t side = 0; side < 3; ++side) {
            std::int32_t& neighbor = current.neighbors[side];
            neighbor = neighbor < 0 ? -1 : remap[static_cast<std::size_t>(neighbor)];
            if (neighbor < 0) {
                walls.push_back({vertexAt(current.vertices[side]), vertexAt(current.vertices[(side + 1) % 3])});
            }
        }
        const std::array<math::Vec2, 3> corners{vertexAt(current.vertices[0]), vertexAt(current.vertices[1]), vertexAt(current.vertices[2])};
        triangleIndex.set(triangle, math::Geometry::bounds(corners));
    }
    dirty = false;
}

void NavMesh::ensureBuilt() {
    if (dirty) {
        build();
    }
}

const std::vector<math::Vec2>& NavMesh::getVertices() {
    ensureBuilt();
    return vertices;
}

const std::vector<NavMesh::Triangle>& NavMesh::getTriangles() {
    ensureBuilt();
    return triangles;
}

bool NavMesh::isInside(std::size_t triangle, math::Vec2 point) const noexcept {
    const Triangle& current = triangles[triangle];
    for (std::size_t side = 0; side < 3; ++side) {
        const math::Vec2 a = vertexAt(current.vertices[side]);
        const math::Vec2 edge = vertexAt(current.vertices[(side + 1) % 3]) - a;
        if (math::Vec2::cross(edge, point - a) < -kEdgeTolerance * edge.getLength()) {
            return false;
        }
    }
    return true;
}

std::optional<std::size_t> NavMesh::findTriangle(math::Vec2 point) {
    ensureBuilt();
    triangleIndex.queryPoint(point, candidates);
    for (const std::uint64_t candidate : candidates) {
        if (isInside(static_cast<std::size_t>(candidate), point)) {
            return static_cast<std::size_t>(candidate);
        }
    }
    return std::nullopt;
}

bool NavMesh::contains(math::Vec2 point) {
    return findTriangle(point).has_value();
}

std::optional<math::Vec2> NavMesh::getClosestPoint(math::Vec2 point) {
    if (contains(point)) {
        return point;
    }
    std::optional<math::Vec2> closest;
    float closestDistance = std::numeric_limits<float>::infinity();
    for (const math::Segment& wall : walls) {
        const math::Vec2 candidate = math::Geometry::closestPoint(wall, point);
        const float distance = math::Vec2::distanceSquared(candidate, point);
        if (distance < closestDistance) {
            closestDistance = distance;
            closest = candidate;
        }
    }
    return closest;
}

float NavMesh::passageWidth(std::size_t triangle, std::size_t entry, std::size_t exit) const noexcept {
    const Triangle& current = triangles[triangle];
    const std::size_t third = 3 - entry - exit;
    const math::Vec2 corner = vertexAt(current.vertices[(third + 2) % 3]);
    const float shorterSide = std::min(math::Vec2::distance(corner, vertexAt(current.vertices[third])), math::Vec2::distance(corner, vertexAt(current.vertices[(third + 1) % 3])));
    return searchWidth(corner, triangle, third, shorterSide);
}

float NavMesh::searchWidth(math::Vec2 corner, std::size_t triangle, std::size_t side, float width) const noexcept {
    const Triangle& current = triangles[triangle];
    const math::Vec2 a = vertexAt(current.vertices[side]);
    const math::Vec2 b = vertexAt(current.vertices[(side + 1) % 3]);
    if (math::Vec2::dot(corner - a, b - a) <= 0.0F || math::Vec2::dot(corner - b, a - b) <= 0.0F) {
        return width;
    }
    const float distance = math::Geometry::distanceToSegment({a, b}, corner);
    if (distance >= width) {
        return width;
    }
    if (current.neighbors[side] < 0) {
        return distance;
    }

    // The edge is open, so a wall closer than the width may lie in the triangle beyond it.
    const auto next = static_cast<std::size_t>(current.neighbors[side]);
    const auto back = static_cast<std::size_t>(std::ranges::find(triangles[next].neighbors, static_cast<std::int32_t>(triangle)) - triangles[next].neighbors.begin());
    return searchWidth(corner, next, (back + 2) % 3, searchWidth(corner, next, (back + 1) % 3, width));
}

bool NavMesh::findCorridor(std::int32_t start, std::int32_t goal, math::Vec2 from, math::Vec2 to, float agentRadius) {
    const std::size_t count = triangles.size();
    if (visits.size() < count) {
        scores.resize(count);
        parents.resize(count);
        entrySides.resize(count);
        entries.resize(count);
        visits.resize(count, 0);
    }
    // Each search stamps the triangles it opens with visit and the ones it settles with visit + 1, so no buffer needs clearing.
    visit += 2;
    if (visit < 2) {
        std::ranges::fill(visits, 0U);
        visit = 2;
    }
    const std::uint32_t settled = visit + 1;
    // clang-format off
    const auto reset = [this, settled](std::int32_t triangle) {
        const auto slot = static_cast<std::size_t>(triangle);
        if (visits[slot] != visit && visits[slot] != settled) {
            visits[slot] = visit;
            scores[slot] = std::numeric_limits<float>::infinity();
            parents[slot] = -1;
            entrySides[slot] = -1;
        }
    };
    // clang-format on

    reset(start);
    scores[static_cast<std::size_t>(start)] = 0.0F;
    entries[static_cast<std::size_t>(start)] = from;
    open.assign(1, {.estimate = math::Vec2::distance(from, to), .triangle = start});
    while (!open.empty()) {
        std::ranges::pop_heap(open, &NavMesh::isWorse);
        const OpenNode current = open.back();
        open.pop_back();
        const auto slot = static_cast<std::size_t>(current.triangle);
        if (visits[slot] == settled) {
            continue;
        }
        visits[slot] = settled;
        if (current.triangle == goal) {
            break;
        }

        const Triangle& triangle = triangles[slot];
        for (std::size_t side = 0; side < 3; ++side) {
            const std::int32_t neighbor = triangle.neighbors[side];
            if (neighbor < 0) {
                continue;
            }
            const math::Vec2 a = vertexAt(triangle.vertices[side]);
            const math::Vec2 b = vertexAt(triangle.vertices[(side + 1) % 3]);
            reset(neighbor);
            const auto next = static_cast<std::size_t>(neighbor);
            if (visits[next] == settled) {
                continue;
            }
            const std::int32_t entry = entrySides[slot];
            const bool narrow = math::Vec2::distance(a, b) < agentRadius * 2.0F || (entry >= 0 && static_cast<std::size_t>(entry) != side && passageWidth(slot, static_cast<std::size_t>(entry), side) < agentRadius * 2.0F);
            if (narrow) {
                continue;
            }

            // Corridors enter each triangle through the middle of the shared edge, which keeps the search cheap and close to the true length.
            const math::Vec2 middle = (a + b) * 0.5F;
            const float score = scores[slot] + math::Vec2::distance(entries[slot], middle);
            if (score < scores[next]) {
                scores[next] = score;
                parents[next] = current.triangle;
                entrySides[next] = static_cast<std::int32_t>(std::ranges::find(triangles[next].neighbors, current.triangle) - triangles[next].neighbors.begin());
                entries[next] = middle;
                open.push_back({.estimate = score + math::Vec2::distance(middle, to), .triangle = neighbor});
                std::ranges::push_heap(open, &NavMesh::isWorse);
            }
        }
    }

    if (visits[static_cast<std::size_t>(goal)] != settled) {
        return false;
    }
    corridor.clear();
    for (std::int32_t triangle = goal; triangle >= 0; triangle = parents[static_cast<std::size_t>(triangle)]) {
        corridor.push_back(triangle);
    }
    std::ranges::reverse(corridor);
    return true;
}

void NavMesh::pullString(math::Vec2 start, math::Vec2 goal, float agentRadius) {
    // Each shared edge becomes a portal, seen from the triangle before it, narrowed by the agent radius at both ends.
    portals.assign(1, {.left = start, .right = start, .leftCorner = start, .rightCorner = start});
    for (std::size_t step = 0; step + 1 < corridor.size(); ++step) {
        const Triangle& triangle = triangles[static_cast<std::size_t>(corridor[step])];
        const auto side = static_cast<std::size_t>(std::ranges::find(triangle.neighbors, corridor[step + 1]) - triangle.neighbors.begin());
        const math::Vec2 left = vertexAt(triangle.vertices[(side + 1) % 3]);
        const math::Vec2 right = vertexAt(triangle.vertices[side]);
        const math::Vec2 inward = (right - left).getNormalized() * agentRadius;
        portals.push_back({.left = left + inward, .right = right - inward, .leftCorner = left, .rightCorner = right});
    }
    portals.push_back({.left = goal, .right = goal, .leftCorner = goal, .rightCorner = goal});

    // A funnel from the apex tightens through the portals and turns at a corner whenever one of its sides crosses the other.
    turns.clear();
    math::Vec2 apex = start;
    math::Vec2 left = start;
    math::Vec2 right = start;
    std::size_t apexIndex = 0;
    std::size_t leftIndex = 0;
    std::size_t rightIndex = 0;

    // A side that never left the apex and the goal add no turn, and a corner the funnel wraps over several portals is one turn.
    // clang-format off
    const auto addTurn = [&](std::size_t portal, bool onLeft) {
        if (portal == apexIndex || portal + 1 == portals.size()) {
            return;
        }
        const math::Vec2 corner = onLeft ? portals[portal].leftCorner : portals[portal].rightCorner;
        if (turns.empty() || turns.back().corner != corner) {
            turns.push_back({.corner = corner, .side = onLeft ? 1.0F : -1.0F});
        }
    };
    // clang-format on
    for (std::size_t index = 1; index < portals.size(); ++index) {
        const Portal& portal = portals[index];
        if (area(apex, right, portal.right) <= 0.0F) {
            if (apex == right || area(apex, left, portal.right) > 0.0F) {
                right = portal.right;
                rightIndex = index;
            } else {
                addTurn(leftIndex, true);
                apex = left;
                apexIndex = leftIndex;
                right = apex;
                rightIndex = apexIndex;
                index = apexIndex;
                continue;
            }
        }
        if (area(apex, left, portal.left) >= 0.0F) {
            if (apex == left || area(apex, right, portal.left) < 0.0F) {
                left = portal.left;
                leftIndex = index;
            } else {
                addTurn(rightIndex, false);
                apex = right;
                apexIndex = rightIndex;
                left = apex;
                leftIndex = apexIndex;
                index = apexIndex;
                continue;
            }
        }
    }
}

math::Vec2 NavMesh::tangentDirection(math::Vec2 from, math::Vec2 to, float offset) noexcept {
    const math::Vec2 toward = to - from;
    const float distance = toward.getLength();
    const float sine = distance > std::fabs(offset) ? offset / distance : std::copysign(1.0F, offset);
    return toward.getNormalized().rotated(-std::asin(sine));
}

void NavMesh::bendAroundTurns(math::Vec2 start, math::Vec2 goal, float agentRadius) {
    // Consecutive turns share the line that touches both circles, on the side of the path each corner needs, and the first and last lines touch the start and the goal.
    directions.clear();
    math::Vec2 from = start;
    float fromOffset = 0.0F;
    for (const Turn& turn : turns) {
        directions.push_back(tangentDirection(from, turn.corner, turn.side * agentRadius - fromOffset));
        from = turn.corner;
        fromOffset = turn.side * agentRadius;
    }
    directions.push_back(tangentDirection(from, goal, -fromOffset));

    // Each turn becomes the corners of a polygon around its circle, whose sides touch the circle, one corner for turns up to a right angle and two for wider ones.
    path.assign(1, start);
    for (std::size_t index = 0; index < turns.size(); ++index) {
        const math::Vec2 in = directions[index];
        const math::Vec2 out = directions[index + 1];
        const float angle = std::atan2(math::Vec2::cross(in, out), math::Vec2::dot(in, out));
        const int count = std::fabs(angle) > std::numbers::pi_v<float> * 0.5F ? 2 : 1;
        const float slice = angle / static_cast<float>(count);
        const float reach = turns[index].side * agentRadius / std::cos(slice * 0.5F);
        for (int part = 0; part < count; ++part) {
            const math::Vec2 point = turns[index].corner - in.rotated(slice * (static_cast<float>(part) + 0.5F)).getPerpendicular() * reach;
            if (path.back() != point) {
                path.push_back(point);
            }
        }
    }
    if (path.back() != goal) {
        path.push_back(goal);
    }

    pathLength = 0.0F;
    for (std::size_t index = 1; index < path.size(); ++index) {
        pathLength += math::Vec2::distance(path[index - 1], path[index]);
    }
}

std::span<const math::Vec2> NavMesh::findPath(math::Vec2 start, math::Vec2 goal, float agentRadius) {
    if (!(agentRadius >= 0.0F) || !std::isfinite(agentRadius)) {
        throw std::invalid_argument("An agent radius must be finite and not negative.");
    }
    path.clear();
    pathLength = std::numeric_limits<float>::infinity();
    const std::optional<std::size_t> first = findTriangle(start);
    const std::optional<std::size_t> last = findTriangle(goal);
    if (!first || !last || !findCorridor(static_cast<std::int32_t>(*first), static_cast<std::int32_t>(*last), start, goal, agentRadius)) {
        return {};
    }
    pullString(start, goal, agentRadius);
    bendAroundTurns(start, goal, agentRadius);
    return path;
}

} // namespace haylen::navigation2d
