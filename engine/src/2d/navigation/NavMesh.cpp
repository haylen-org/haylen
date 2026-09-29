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

    // A triangle is walkable when it lies inside the boundary and outside every obstacle, which also merges overlapping obstacles.
    ConstrainedTriangulation triangulation;
    // clang-format off
    triangulation.build(segments, [this](math::Vec2 point) {
        return math::Geometry::contains(boundary, point) && std::ranges::none_of(obstacles, [point](const auto& obstacle) { return math::Geometry::contains(obstacle.second, point); });
    });
    // clang-format on
    vertices = triangulation.getVertices();
    triangles.clear();
    for (const ConstrainedTriangulation::Triangle& triangle : triangulation.getTriangles()) {
        const std::array<std::int32_t, 3>& corners = triangle.vertices;
        triangles.push_back({.vertices = {static_cast<std::uint32_t>(corners[0]), static_cast<std::uint32_t>(corners[1]), static_cast<std::uint32_t>(corners[2])}, .neighbors = triangle.neighbors});
    }

    walls.clear();
    triangleIndex.clear();
    for (std::size_t triangle = 0; triangle < triangles.size(); ++triangle) {
        const Triangle& current = triangles[triangle];
        for (std::size_t side = 0; side < 3; ++side) {
            if (current.neighbors[side] < 0) {
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

void NavMesh::gatherWalls(math::Vec2 point, float distance) {
    triangleIndex.queryCircle(point, distance, candidates);
    nearWalls.clear();
    for (const std::uint64_t candidate : candidates) {
        const Triangle& triangle = triangles[static_cast<std::size_t>(candidate)];
        for (std::size_t side = 0; side < 3; ++side) {
            if (triangle.neighbors[side] < 0) {
                nearWalls.push_back({vertexAt(triangle.vertices[side]), vertexAt(triangle.vertices[(side + 1) % 3])});
            }
        }
    }
}

bool NavMesh::hasRoom(math::Vec2 spot, float agentRadius) const noexcept {
    return std::ranges::none_of(nearWalls, [&](const math::Segment& wall) { return math::Geometry::distanceToSegment(wall, spot) < agentRadius - kEdgeTolerance; });
}

bool NavMesh::canStep(math::Vec2 from, math::Vec2 to) const noexcept {
    // clang-format off
    return std::ranges::none_of(nearWalls, [&](const math::Segment& wall) {
        const std::optional<math::Vec2> crossing = math::Geometry::intersection({from, to}, wall);
        return crossing && math::Vec2::distance(*crossing, from) > kEdgeTolerance;
    });
    // clang-format on
}

void NavMesh::addCrossings(const math::Segment& piece, math::Vec2 center, float radius, std::vector<math::Vec2>& crossings) {
    const math::Vec2 direction = piece.end - piece.start;
    const float length = direction.getLength();
    if (length <= 0.0F) {
        return;
    }
    const math::Vec2 along = direction / length;
    const math::Vec2 offset = piece.start - center;
    const float middle = -math::Vec2::dot(along, offset);
    const float squared = middle * middle - offset.getLengthSquared() + radius * radius;
    if (squared < 0.0F) {
        return;
    }
    for (const float travel : {middle - std::sqrt(squared), middle + std::sqrt(squared)}) {
        if (travel >= 0.0F && travel <= length) {
            crossings.push_back(piece.start + along * travel);
        }
    }
}

void NavMesh::addCrossings(math::Vec2 center, math::Vec2 other, float radius, std::vector<math::Vec2>& crossings) {
    const float half = math::Vec2::distance(center, other) * 0.5F;
    if (half <= 0.0F || half > radius) {
        return;
    }
    const math::Vec2 middle = (center + other) * 0.5F;
    const math::Vec2 across = (other - center).getNormalized().getPerpendicular() * std::sqrt(radius * radius - half * half);
    crossings.push_back(middle + across);
    crossings.push_back(middle - across);
}

std::optional<math::Vec2> NavMesh::findRoom(math::Vec2 point, float agentRadius) {
    if (!contains(point)) {
        return std::nullopt;
    }
    const float reach = agentRadius * 2.0F;
    gatherWalls(point, reach + agentRadius);
    if (hasRoom(point, agentRadius)) {
        return point;
    }

    // Every spot the radius away from the walls lies on the walls moved by the radius to their walkable side or on the circles of the radius around their ends. The closest one is the foot of the point on one of those pieces or a point where two of them cross.
    offsets.clear();
    spots.clear();
    for (const math::Segment& wall : nearWalls) {
        const math::Vec2 normal = (wall.end - wall.start).getNormalized().getPerpendicular() * agentRadius;
        offsets.push_back({wall.start + normal, wall.end + normal});
    }
    for (std::size_t index = 0; index < offsets.size(); ++index) {
        const math::Vec2 corner = nearWalls[index].start;
        spots.push_back(math::Geometry::closestPoint(offsets[index], point));
        spots.push_back(corner + (point - corner).getNormalized() * agentRadius);
        for (std::size_t other = index + 1; other < offsets.size(); ++other) {
            if (const std::optional<math::Vec2> crossing = math::Geometry::intersection(offsets[index], offsets[other])) {
                spots.push_back(*crossing);
            }
            addCrossings(offsets[index], nearWalls[other].start, agentRadius, spots);
            addCrossings(offsets[other], corner, agentRadius, spots);
            addCrossings(corner, nearWalls[other].start, agentRadius, spots);
        }
    }

    std::optional<math::Vec2> room;
    float roomDistance = reach;
    for (const math::Vec2 spot : spots) {
        const float distance = math::Vec2::distance(point, spot);
        if (distance <= roomDistance && hasRoom(spot, agentRadius) && contains(spot)) {
            room = spot;
            roomDistance = distance;
        }
    }
    if (!room || !canStep(point, *room)) {
        return std::nullopt;
    }
    return room;
}

bool NavMesh::findCorridor(std::int32_t start, std::int32_t goal, math::Vec2 from, math::Vec2 to, float agentRadius) {
    const std::size_t count = triangles.size();
    if (visits.size() < count) {
        scores.resize(count);
        parents.resize(count);
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
            if (math::Vec2::distance(a, b) < agentRadius * 2.0F) {
                continue;
            }

            // Corridors enter each triangle through the middle of the shared edge, which keeps the search cheap and close to the true length.
            const math::Vec2 middle = (a + b) * 0.5F;
            const float score = scores[slot] + math::Vec2::distance(entries[slot], middle);
            if (score < scores[next]) {
                scores[next] = score;
                parents[next] = current.triangle;
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

    // A side that never left the apex and the goal add no turn, and a corner the funnel wraps over several portals is one turn. A portal end that widens a side by less than the edge tolerance still tightens it, so the rounded vertices along a straight wall never become turns.
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
        if (area(apex, right, portal.right) <= kEdgeTolerance * math::Vec2::distance(apex, portal.right)) {
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
        if (area(apex, left, portal.left) >= -kEdgeTolerance * math::Vec2::distance(apex, portal.left)) {
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
    if (path.back() != start) {
        path.push_back(start);
    }
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
}

std::span<const math::Vec2> NavMesh::findPath(math::Vec2 start, math::Vec2 goal, float agentRadius) {
    if (!(agentRadius >= 0.0F) || !std::isfinite(agentRadius)) {
        throw std::invalid_argument("An agent radius must be finite and not negative.");
    }
    path.clear();
    pathLength = std::numeric_limits<float>::infinity();
    const std::optional<math::Vec2> from = findRoom(start, agentRadius);
    const std::optional<math::Vec2> to = findRoom(goal, agentRadius);
    if (!from || !to) {
        return {};
    }
    const std::optional<std::size_t> first = findTriangle(*from);
    const std::optional<std::size_t> last = findTriangle(*to);
    if (!first || !last || !findCorridor(static_cast<std::int32_t>(*first), static_cast<std::int32_t>(*last), *from, *to, agentRadius)) {
        return {};
    }

    pullString(*from, *to, agentRadius);
    path.assign(1, start);
    bendAroundTurns(*from, *to, agentRadius);
    if (path.back() != goal) {
        path.push_back(goal);
    }
    pathLength = 0.0F;
    for (std::size_t index = 1; index < path.size(); ++index) {
        pathLength += math::Vec2::distance(path[index - 1], path[index]);
    }
    return path;
}

} // namespace haylen::navigation2d
