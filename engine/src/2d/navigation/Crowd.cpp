#include "haylen/2d/navigation/Crowd.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

#include "haylen/core/JobSystem.hpp"
#include "haylen/math/Geometry.hpp"

namespace haylen::navigation2d {

const Crowd::Flocking Crowd::kDefaultFlocking{};

Crowd::Crowd(const Flocking& value) : flocking(value) {}

float Crowd::leftOf(math::Vec2 a, math::Vec2 b, math::Vec2 c) noexcept {
    return math::Vec2::cross(a - c, b - a);
}

void Crowd::requireFinite(math::Vec2 value) {
    if (!std::isfinite(value.x) || !std::isfinite(value.y)) {
        throw std::invalid_argument("Crowd positions, targets and velocities must be finite.");
    }
}

std::uint32_t Crowd::addAgent(const AgentOptions& options) {
    const bool valid = options.radius >= 0.0F && options.maxSpeed >= 0.0F && options.neighborDistance >= 0.0F && options.timeHorizon > 0.0F && options.obstacleTimeHorizon > 0.0F;
    const bool finite = std::isfinite(options.position.x) && std::isfinite(options.position.y) && std::isfinite(options.velocity.x) && std::isfinite(options.velocity.y) && std::isfinite(options.radius) && std::isfinite(options.maxSpeed) && std::isfinite(options.neighborDistance);
    if (!valid || !finite) {
        throw std::invalid_argument("A crowd agent needs a finite position, velocity, radius, speed and neighbor distance and positive time horizons.");
    }

    std::uint32_t id = 0;
    if (freeAgents.empty()) {
        id = static_cast<std::uint32_t>(agents.size());
        agents.emplace_back();
    } else {
        id = freeAgents.back();
        freeAgents.pop_back();
    }
    agents[id] = {.options = options, .preferred = {}, .target = std::nullopt, .next = options.velocity, .active = true};
    ++activeAgents;
    return id;
}

bool Crowd::removeAgent(std::uint32_t id) {
    if (!hasAgent(id)) {
        return false;
    }
    agents[id].active = false;
    freeAgents.push_back(id);
    --activeAgents;
    return true;
}

bool Crowd::hasAgent(std::uint32_t id) const noexcept {
    return id < agents.size() && agents[id].active;
}

Crowd::Agent& Crowd::agentAt(std::uint32_t id) {
    if (!hasAgent(id)) {
        throw std::out_of_range("Agent " + std::to_string(id) + " is not in the crowd.");
    }
    return agents[id];
}

const Crowd::Agent& Crowd::agentAt(std::uint32_t id) const {
    if (!hasAgent(id)) {
        throw std::out_of_range("Agent " + std::to_string(id) + " is not in the crowd.");
    }
    return agents[id];
}

void Crowd::addObstacle(std::span<const math::Vec2> polygon) {
    if (polygon.size() < 2) {
        throw std::invalid_argument("A crowd obstacle needs at least two points.");
    }
    if (!std::ranges::all_of(polygon, [](math::Vec2 point) { return std::isfinite(point.x) && std::isfinite(point.y); })) {
        throw std::invalid_argument("A crowd obstacle needs finite points.");
    }

    // The solver expects solid outlines with a positive signed area, whose outside lies to the right of every edge.
    std::vector<math::Vec2> points(polygon.begin(), polygon.end());
    if (points.size() > 2 && math::Geometry::signedArea(points) < 0.0F) {
        std::ranges::reverse(points);
    }

    const std::size_t first = vertices.size();
    const std::size_t count = points.size();
    for (std::size_t index = 0; index < count; ++index) {
        const std::size_t previous = index == 0 ? count - 1 : index - 1;
        const std::size_t next = (index + 1) % count;
        vertices.push_back({.point = points[index], .direction = (points[next] - points[index]).getNormalized(), .next = first + next, .previous = first + previous, .convex = count == 2 || leftOf(points[previous], points[index], points[next]) >= 0.0F});
    }
}

void Crowd::clearObstacles() noexcept {
    vertices.clear();
}

math::Vec2 Crowd::getPosition(std::uint32_t id) const {
    return agentAt(id).options.position;
}

void Crowd::setPosition(std::uint32_t id, math::Vec2 value) {
    Agent& agent = agentAt(id);
    requireFinite(value);
    agent.options.position = value;
}

math::Vec2 Crowd::getVelocity(std::uint32_t id) const {
    return agentAt(id).options.velocity;
}

float Crowd::getRadius(std::uint32_t id) const {
    return agentAt(id).options.radius;
}

float Crowd::getMaxSpeed(std::uint32_t id) const {
    return agentAt(id).options.maxSpeed;
}

void Crowd::setPreferredVelocity(std::uint32_t id, math::Vec2 value) {
    Agent& agent = agentAt(id);
    requireFinite(value);
    agent.preferred = value;
    agent.target.reset();
}

math::Vec2 Crowd::getPreferredVelocity(std::uint32_t id) const {
    return agentAt(id).preferred;
}

void Crowd::setTarget(std::uint32_t id, math::Vec2 value) {
    Agent& agent = agentAt(id);
    requireFinite(value);
    agent.target = value;
    agent.preferred = {};
}

void Crowd::clearTarget(std::uint32_t id) {
    agentAt(id).target.reset();
}

std::optional<math::Vec2> Crowd::getTarget(std::uint32_t id) const {
    return agentAt(id).target;
}

void Crowd::bucketAgents() {
    math::Vec2 low{std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()};
    math::Vec2 high = -low;
    float reach = 1.0F;
    for (const Agent& agent : agents) {
        if (agent.active) {
            low = math::Vec2::min(low, agent.options.position);
            high = math::Vec2::max(high, agent.options.position);
            reach = std::max(reach, agent.options.neighborDistance);
        }
    }
    if (activeAgents == 0) {
        bucketsX = 0;
        bucketsY = 0;
        return;
    }

    // Buckets are at least as wide as the widest neighbor distance, and grow further when agents spread over a huge area.
    bucketOrigin = low;
    bucketSize = reach;
    const auto limit = static_cast<float>(activeAgents * 4 + 16);
    while (std::floor((high.x - low.x) / bucketSize + 1.0F) * std::floor((high.y - low.y) / bucketSize + 1.0F) > limit) {
        bucketSize *= 2.0F;
    }
    bucketsX = static_cast<int>((high.x - low.x) / bucketSize) + 1;
    bucketsY = static_cast<int>((high.y - low.y) / bucketSize) + 1;

    const auto bucketOf = [this](math::Vec2 position) { return static_cast<std::size_t>(static_cast<int>((position.y - bucketOrigin.y) / bucketSize) * bucketsX + static_cast<int>((position.x - bucketOrigin.x) / bucketSize)); };
    const auto bucketCount = static_cast<std::size_t>(bucketsX) * static_cast<std::size_t>(bucketsY);
    bucketStarts.assign(bucketCount + 1, 0);
    for (const Agent& agent : agents) {
        if (agent.active) {
            ++bucketStarts[bucketOf(agent.options.position) + 1];
        }
    }
    for (std::size_t bucket = 0; bucket < bucketCount; ++bucket) {
        bucketStarts[bucket + 1] += bucketStarts[bucket];
    }
    bucketFill.assign(bucketStarts.begin(), bucketStarts.end() - 1);
    bucketAgentIds.resize(activeAgents);
    for (std::uint32_t id = 0; id < agents.size(); ++id) {
        if (agents[id].active) {
            bucketAgentIds[bucketFill[bucketOf(agents[id].options.position)]++] = id;
        }
    }
}

void Crowd::findNeighbors(std::uint32_t id, Scratch& scratch) const {
    const AgentOptions& self = agents[id].options;
    scratch.neighbors.clear();
    const float range = self.neighborDistance;
    if (self.maxNeighbors == 0 || range <= 0.0F) {
        return;
    }

    const auto cellOf = [this](float value, float origin, int count) { return std::clamp(static_cast<int>(std::floor((value - origin) / bucketSize)), 0, count - 1); };
    const int left = cellOf(self.position.x - range, bucketOrigin.x, bucketsX);
    const int right = cellOf(self.position.x + range, bucketOrigin.x, bucketsX);
    const int top = cellOf(self.position.y - range, bucketOrigin.y, bucketsY);
    const int bottom = cellOf(self.position.y + range, bucketOrigin.y, bucketsY);

    // Keeps the closest neighbors sorted by distance and then by id, so every step sees them in the same order.
    for (int y = top; y <= bottom; ++y) {
        for (int x = left; x <= right; ++x) {
            const auto bucket = static_cast<std::size_t>(y * bucketsX + x);
            for (std::uint32_t slot = bucketStarts[bucket]; slot < bucketStarts[bucket + 1]; ++slot) {
                const std::uint32_t other = bucketAgentIds[slot];
                const float distanceSquared = math::Vec2::distanceSquared(self.position, agents[other].options.position);
                if (other == id || distanceSquared >= range * range) {
                    continue;
                }
                const std::pair<float, std::uint32_t> candidate{distanceSquared, other};
                if (scratch.neighbors.size() == self.maxNeighbors) {
                    if (!(candidate < scratch.neighbors.back())) {
                        continue;
                    }
                    scratch.neighbors.pop_back();
                }
                scratch.neighbors.insert(std::ranges::upper_bound(scratch.neighbors, candidate), candidate);
            }
        }
    }
}

void Crowd::findWalls(std::uint32_t id, Scratch& scratch) const {
    const AgentOptions& self = agents[id].options;
    scratch.walls.clear();
    const float range = self.obstacleTimeHorizon * self.maxSpeed + self.radius;
    for (std::size_t index = 0; index < vertices.size(); ++index) {
        const math::Vec2 a = vertices[index].point;
        const math::Vec2 b = vertices[vertices[index].next].point;
        const math::Vec2 edge = b - a;
        const float side = leftOf(a, b, self.position);

        // Only edges that face the agent and come within range can block it.
        if (side >= 0.0F || side * side / edge.getLengthSquared() >= range * range) {
            continue;
        }
        const float along = std::clamp(math::Vec2::dot(self.position - a, edge) / edge.getLengthSquared(), 0.0F, 1.0F);
        const float distanceSquared = math::Vec2::distanceSquared(self.position, a + edge * along);
        if (distanceSquared < range * range) {
            scratch.walls.emplace_back(distanceSquared, index);
        }
    }
    std::ranges::sort(scratch.walls);
}

math::Vec2 Crowd::preferredVelocity(std::uint32_t id, const Scratch& scratch, float deltaSeconds) const noexcept {
    const Agent& agent = agents[id];
    const AgentOptions& self = agent.options;
    math::Vec2 preferred = agent.preferred;
    if (agent.target) {
        // Double precision keeps the distance to targets far across the float range from overflowing.
        const double offsetX = static_cast<double>(agent.target->x) - self.position.x;
        const double offsetY = static_cast<double>(agent.target->y) - self.position.y;
        const double distance = std::sqrt(offsetX * offsetX + offsetY * offsetY);
        const double speed = std::min(static_cast<double>(self.maxSpeed), distance / deltaSeconds);
        preferred = distance > kEpsilon ? math::Vec2{static_cast<float>(offsetX / distance * speed), static_cast<float>(offsetY / distance * speed)} : math::Vec2{};
    }
    const bool flocks = flocking.separation != 0.0F || flocking.alignment != 0.0F || flocking.cohesion != 0.0F;
    if (!flocks || scratch.neighbors.empty()) {
        return preferred.clampedLength(self.maxSpeed);
    }

    math::Vec2 away;
    math::Vec2 heading;
    math::Vec2 center;
    for (const auto& [distanceSquared, other] : scratch.neighbors) {
        const AgentOptions& neighbor = agents[other].options;
        const float distance = std::sqrt(distanceSquared);
        if (distance > 0.0F) {
            away += (self.position - neighbor.position) / distance * (1.0F - distance / self.neighborDistance);
        }
        heading += neighbor.velocity;
        center += neighbor.position;
    }
    const auto count = static_cast<float>(scratch.neighbors.size());
    preferred += away * (self.maxSpeed * flocking.separation);
    preferred += (heading / count - self.velocity) * flocking.alignment;
    preferred += (center / count - self.position).getNormalized() * (self.maxSpeed * flocking.cohesion);
    return preferred.clampedLength(self.maxSpeed);
}

void Crowd::addWallLines(const Agent& agent, Scratch& scratch) const {
    const AgentOptions& self = agent.options;
    const float inverseHorizon = 1.0F / self.obstacleTimeHorizon;
    const float radiusSquared = self.radius * self.radius;
    const float infinity = std::numeric_limits<float>::infinity();

    for (const auto& [ignored, index] : scratch.walls) {
        const Vertex* first = &vertices[index];
        const Vertex* second = &vertices[first->next];
        const math::Vec2 relative1 = first->point - self.position;
        const math::Vec2 relative2 = second->point - self.position;

        // An edge whose velocity obstacle the lines already cut off adds nothing.
        const bool covered = std::ranges::any_of(scratch.lines, [&](const Line& line) { return math::Vec2::cross(relative1 * inverseHorizon - line.point, line.direction) - inverseHorizon * self.radius >= -kEpsilon && math::Vec2::cross(relative2 * inverseHorizon - line.point, line.direction) - inverseHorizon * self.radius >= -kEpsilon; });
        if (covered) {
            continue;
        }

        // An agent already touching the edge or one of its ends backs away from it.
        const float distanceSquared1 = relative1.getLengthSquared();
        const float distanceSquared2 = relative2.getLengthSquared();
        const math::Vec2 edge = second->point - first->point;
        const float along = math::Vec2::dot(-relative1, edge) / edge.getLengthSquared();
        const float distanceSquaredLine = (-relative1 - edge * along).getLengthSquared();
        if (along < 0.0F && distanceSquared1 <= radiusSquared) {
            if (first->convex) {
                scratch.lines.push_back({.point = {}, .direction = math::Vec2{-relative1.y, relative1.x}.getNormalized()});
            }
            continue;
        }
        if (along > 1.0F && distanceSquared2 <= radiusSquared) {
            if (second->convex && math::Vec2::cross(relative2, second->direction) >= 0.0F) {
                scratch.lines.push_back({.point = {}, .direction = math::Vec2{-relative2.y, relative2.x}.getNormalized()});
            }
            continue;
        }
        if (along >= 0.0F && along < 1.0F && distanceSquaredLine <= radiusSquared) {
            scratch.lines.push_back({.point = {}, .direction = -first->direction});
            continue;
        }

        // The legs of the velocity obstacle graze the circle of the agent around each end, and an oblique view lets one end define both.
        math::Vec2 leftLeg;
        math::Vec2 rightLeg;
        if (along < 0.0F && distanceSquaredLine <= radiusSquared) {
            if (!first->convex) {
                continue;
            }
            second = first;
            const float leg = std::sqrt(distanceSquared1 - radiusSquared);
            leftLeg = math::Vec2{relative1.x * leg - relative1.y * self.radius, relative1.x * self.radius + relative1.y * leg} / distanceSquared1;
            rightLeg = math::Vec2{relative1.x * leg + relative1.y * self.radius, -relative1.x * self.radius + relative1.y * leg} / distanceSquared1;
        } else if (along > 1.0F && distanceSquaredLine <= radiusSquared) {
            if (!second->convex) {
                continue;
            }
            first = second;
            const float leg = std::sqrt(distanceSquared2 - radiusSquared);
            leftLeg = math::Vec2{relative2.x * leg - relative2.y * self.radius, relative2.x * self.radius + relative2.y * leg} / distanceSquared2;
            rightLeg = math::Vec2{relative2.x * leg + relative2.y * self.radius, -relative2.x * self.radius + relative2.y * leg} / distanceSquared2;
        } else {
            if (first->convex) {
                const float leg = std::sqrt(distanceSquared1 - radiusSquared);
                leftLeg = math::Vec2{relative1.x * leg - relative1.y * self.radius, relative1.x * self.radius + relative1.y * leg} / distanceSquared1;
            } else {
                leftLeg = -first->direction;
            }
            if (second->convex) {
                const float leg = std::sqrt(distanceSquared2 - radiusSquared);
                rightLeg = math::Vec2{relative2.x * leg + relative2.y * self.radius, -relative2.x * self.radius + relative2.y * leg} / distanceSquared2;
            } else {
                rightLeg = first->direction;
            }
        }

        // A leg of a convex end never points into the neighboring edge, which cuts it off instead.
        const Vertex& leftNeighbor = vertices[first->previous];
        bool leftForeign = false;
        bool rightForeign = false;
        if (first->convex && math::Vec2::cross(leftLeg, -leftNeighbor.direction) >= 0.0F) {
            leftLeg = -leftNeighbor.direction;
            leftForeign = true;
        }
        if (second->convex && math::Vec2::cross(rightLeg, second->direction) <= 0.0F) {
            rightLeg = second->direction;
            rightForeign = true;
        }

        // Projects the current velocity on the cutoff circles, the cutoff line or the legs, whichever is closest.
        const math::Vec2 leftCutoff = (first->point - self.position) * inverseHorizon;
        const math::Vec2 rightCutoff = (second->point - self.position) * inverseHorizon;
        const math::Vec2 cutoff = rightCutoff - leftCutoff;
        const bool sameVertex = first == second;
        const float t = sameVertex ? 0.5F : math::Vec2::dot(self.velocity - leftCutoff, cutoff) / cutoff.getLengthSquared();
        const float tLeft = math::Vec2::dot(self.velocity - leftCutoff, leftLeg);
        const float tRight = math::Vec2::dot(self.velocity - rightCutoff, rightLeg);
        if ((t < 0.0F && tLeft < 0.0F) || (sameVertex && tLeft < 0.0F && tRight < 0.0F)) {
            const math::Vec2 unit = (self.velocity - leftCutoff).getNormalized();
            scratch.lines.push_back({.point = leftCutoff + unit * (self.radius * inverseHorizon), .direction = {unit.y, -unit.x}});
            continue;
        }
        if (t > 1.0F && tRight < 0.0F) {
            const math::Vec2 unit = (self.velocity - rightCutoff).getNormalized();
            scratch.lines.push_back({.point = rightCutoff + unit * (self.radius * inverseHorizon), .direction = {unit.y, -unit.x}});
            continue;
        }

        const float cutoffDistance = t < 0.0F || t > 1.0F || sameVertex ? infinity : (self.velocity - (leftCutoff + cutoff * t)).getLengthSquared();
        const float leftDistance = tLeft < 0.0F ? infinity : (self.velocity - (leftCutoff + leftLeg * tLeft)).getLengthSquared();
        const float rightDistance = tRight < 0.0F ? infinity : (self.velocity - (rightCutoff + rightLeg * tRight)).getLengthSquared();
        if (cutoffDistance <= leftDistance && cutoffDistance <= rightDistance) {
            const math::Vec2 direction = -first->direction;
            scratch.lines.push_back({.point = leftCutoff + math::Vec2{-direction.y, direction.x} * (self.radius * inverseHorizon), .direction = direction});
        } else if (leftDistance <= rightDistance) {
            if (!leftForeign) {
                scratch.lines.push_back({.point = leftCutoff + math::Vec2{-leftLeg.y, leftLeg.x} * (self.radius * inverseHorizon), .direction = leftLeg});
            }
        } else if (!rightForeign) {
            const math::Vec2 direction = -rightLeg;
            scratch.lines.push_back({.point = rightCutoff + math::Vec2{-direction.y, direction.x} * (self.radius * inverseHorizon), .direction = direction});
        }
    }
}

void Crowd::addAgentLines(std::uint32_t id, Scratch& scratch, float deltaSeconds) const {
    const AgentOptions& self = agents[id].options;
    const float inverseHorizon = 1.0F / self.timeHorizon;
    for (const auto& [distanceSquared, otherId] : scratch.neighbors) {
        const AgentOptions& other = agents[otherId].options;
        const math::Vec2 relativePosition = other.position - self.position;
        const math::Vec2 relativeVelocity = self.velocity - other.velocity;
        const float combinedRadius = self.radius + other.radius;
        const float combinedSquared = combinedRadius * combinedRadius;
        Line line;
        math::Vec2 change;

        if (distanceSquared > combinedSquared) {
            const math::Vec2 w = relativeVelocity - relativePosition * inverseHorizon;
            const float wLengthSquared = w.getLengthSquared();
            const float projection = math::Vec2::dot(w, relativePosition);
            if (projection < 0.0F && projection * projection > combinedSquared * wLengthSquared) {
                // The relative velocity falls behind the cutoff circle, so the line touches that circle.
                const float wLength = std::sqrt(wLengthSquared);
                const math::Vec2 unit = w / wLength;
                line.direction = {unit.y, -unit.x};
                change = unit * (combinedRadius * inverseHorizon - wLength);
            } else {
                // Otherwise the line runs along the closer leg of the cone.
                const float leg = std::sqrt(distanceSquared - combinedSquared);
                if (math::Vec2::cross(relativePosition, w) > 0.0F) {
                    line.direction = math::Vec2{relativePosition.x * leg - relativePosition.y * combinedRadius, relativePosition.x * combinedRadius + relativePosition.y * leg} / distanceSquared;
                } else {
                    line.direction = -math::Vec2{relativePosition.x * leg + relativePosition.y * combinedRadius, -relativePosition.x * combinedRadius + relativePosition.y * leg} / distanceSquared;
                }
                change = line.direction * math::Vec2::dot(relativeVelocity, line.direction) - relativeVelocity;
            }
        } else {
            // Overlapping agents separate within one step, and agents on the same spot part along x by id.
            const float inverseStep = 1.0F / deltaSeconds;
            const math::Vec2 w = relativeVelocity - relativePosition * inverseStep;
            const float wLength = w.getLength();
            const math::Vec2 unit = wLength > kEpsilon ? w / wLength : math::Vec2{id < otherId ? -1.0F : 1.0F, 0.0F};
            line.direction = {unit.y, -unit.x};
            change = unit * (combinedRadius * inverseStep - wLength);
        }
        line.point = self.velocity + change * 0.5F;
        scratch.lines.push_back(line);
    }
}

bool Crowd::solveLine(std::span<const Line> lines, std::size_t lineIndex, float radius, math::Vec2 optimal, bool directionOptimal, math::Vec2& result) noexcept {
    const Line& line = lines[lineIndex];
    const float projection = math::Vec2::dot(line.point, line.direction);
    const float discriminant = projection * projection + radius * radius - line.point.getLengthSquared();
    if (discriminant < 0.0F) {
        return false;
    }

    // Clips the line to the speed circle and then to every earlier half-plane.
    const float root = std::sqrt(discriminant);
    float leftLimit = -projection - root;
    float rightLimit = -projection + root;
    for (std::size_t index = 0; index < lineIndex; ++index) {
        const float denominator = math::Vec2::cross(line.direction, lines[index].direction);
        const float numerator = math::Vec2::cross(lines[index].direction, line.point - lines[index].point);
        if (std::fabs(denominator) <= kEpsilon) {
            if (numerator < 0.0F) {
                return false;
            }
            continue;
        }
        const float limit = numerator / denominator;
        if (denominator >= 0.0F) {
            rightLimit = std::min(rightLimit, limit);
        } else {
            leftLimit = std::max(leftLimit, limit);
        }
        if (leftLimit > rightLimit) {
            return false;
        }
    }

    if (directionOptimal) {
        result = line.point + line.direction * (math::Vec2::dot(optimal, line.direction) > 0.0F ? rightLimit : leftLimit);
        return true;
    }
    const float along = std::clamp(math::Vec2::dot(line.direction, optimal - line.point), leftLimit, rightLimit);
    result = line.point + line.direction * along;
    return true;
}

std::size_t Crowd::solvePlanes(std::span<const Line> lines, float radius, math::Vec2 optimal, bool directionOptimal, math::Vec2& result) noexcept {
    if (directionOptimal) {
        result = optimal * radius;
    } else {
        result = optimal.clampedLength(radius);
    }

    for (std::size_t index = 0; index < lines.size(); ++index) {
        if (math::Vec2::cross(lines[index].direction, lines[index].point - result) > 0.0F) {
            const math::Vec2 previous = result;
            if (!solveLine(lines, index, radius, optimal, directionOptimal, result)) {
                result = previous;
                return index;
            }
        }
    }
    return lines.size();
}

void Crowd::solveFallback(std::span<const Line> lines, std::size_t obstacleLines, std::size_t beginLine, float radius, std::vector<Line>& projected, math::Vec2& result) {
    // When the constraints leave no room, the velocity that least violates the agent lines wins, while obstacle lines stay hard.
    float distance = 0.0F;
    for (std::size_t index = beginLine; index < lines.size(); ++index) {
        if (math::Vec2::cross(lines[index].direction, lines[index].point - result) <= distance) {
            continue;
        }

        projected.assign(lines.begin(), lines.begin() + static_cast<std::ptrdiff_t>(obstacleLines));
        for (std::size_t other = obstacleLines; other < index; ++other) {
            Line line;
            const float determinant = math::Vec2::cross(lines[index].direction, lines[other].direction);
            if (std::fabs(determinant) <= kEpsilon) {
                if (math::Vec2::dot(lines[index].direction, lines[other].direction) > 0.0F) {
                    continue;
                }
                line.point = (lines[index].point + lines[other].point) * 0.5F;
            } else {
                line.point = lines[index].point + lines[index].direction * (math::Vec2::cross(lines[other].direction, lines[index].point - lines[other].point) / determinant);
            }
            line.direction = (lines[other].direction - lines[index].direction).getNormalized();
            projected.push_back(line);
        }

        const math::Vec2 previous = result;
        if (solvePlanes(projected, radius, {-lines[index].direction.y, lines[index].direction.x}, true, result) < projected.size()) {
            result = previous;
        }
        distance = math::Vec2::cross(lines[index].direction, lines[index].point - result);
    }
}

void Crowd::computeVelocity(std::uint32_t id, Scratch& scratch, float deltaSeconds) {
    findNeighbors(id, scratch);
    findWalls(id, scratch);
    const math::Vec2 preferred = preferredVelocity(id, scratch, deltaSeconds);

    scratch.lines.clear();
    addWallLines(agents[id], scratch);
    const std::size_t obstacleLines = scratch.lines.size();
    addAgentLines(id, scratch, deltaSeconds);

    const float maxSpeed = agents[id].options.maxSpeed;
    math::Vec2 result;
    const std::size_t failed = solvePlanes(scratch.lines, maxSpeed, preferred, false, result);
    if (failed < scratch.lines.size()) {
        solveFallback(scratch.lines, obstacleLines, failed, maxSpeed, scratch.projected, result);
    }
    agents[id].next = result;
}

void Crowd::step(float deltaSeconds, core::JobSystem* jobs) {
    if (!(deltaSeconds > 0.0F) || !std::isfinite(deltaSeconds)) {
        throw std::invalid_argument("A crowd step needs a positive and finite time.");
    }
    bucketAgents();

    // Every agent picks its velocity from the state before the step, so the chunks may run in any order on any thread with the same result.
    const std::size_t chunks = jobs == nullptr ? 1 : std::clamp<std::size_t>(activeAgents / kChunkAgents, 1, jobs->getWorkerCount());
    if (scratches.size() < chunks) {
        scratches.resize(chunks);
    }
    const std::size_t chunkSize = (agents.size() + chunks - 1) / chunks;
    // clang-format off
    const auto runChunks = [&](std::size_t begin, std::size_t end) {
        for (std::size_t chunk = begin; chunk < end; ++chunk) {
            for (std::size_t id = chunk * chunkSize; id < std::min(agents.size(), (chunk + 1) * chunkSize); ++id) {
                if (agents[id].active) {
                    computeVelocity(static_cast<std::uint32_t>(id), scratches[chunk], deltaSeconds);
                }
            }
        }
    };
    // clang-format on
    if (chunks > 1) {
        jobs->parallelFor(0, chunks, 1, runChunks);
    } else {
        runChunks(0, 1);
    }

    for (Agent& agent : agents) {
        if (agent.active) {
            agent.options.velocity = agent.next;
            agent.options.position += agent.next * deltaSeconds;
        }
    }
}

} // namespace haylen::navigation2d
