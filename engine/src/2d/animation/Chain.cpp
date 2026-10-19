#include "haylen/2d/animation/Chain.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace haylen::animation2d {

Chain::Chain(const Options& options) : points(options.lengths.size() + 1), lengths(options.lengths) {
    if (lengths.empty()) {
        throw std::invalid_argument("A chain needs at least one segment.");
    }
    if (std::ranges::any_of(lengths, [](float length) { return !(length > 0.0F) || !std::isfinite(length); })) {
        throw std::invalid_argument("Every segment of a chain needs a finite length greater than zero.");
    }

    totalLength = std::accumulate(lengths.begin(), lengths.end(), 0.0F);
    setMaxBend(options.maxBend);
    points.front() = options.root;
    straighten(options.angle);
}

bool Chain::reach(math::Vec2 target, const Reach& options) {
    const math::Vec2 root = points.front();
    if (math::Vec2::distance(root, target) >= totalLength) {
        const math::Vec2 direction = directionOr(target - root, points[1] - root);
        for (std::size_t index = 0; index < lengths.size(); ++index) {
            points[index + 1] = points[index] + direction * lengths[index];
        }
        return math::Vec2::distance(points.back(), target) <= options.tolerance;
    }

    if (options.pole) {
        bendTowardPole(target, *options.pole);
    }
    for (int iteration = 0; iteration < options.iterations; ++iteration) {
        if (math::Vec2::distance(points.back(), target) <= options.tolerance) {
            return true;
        }
        pullToward(target);
        pushFromRoot(root);
    }
    return math::Vec2::distance(points.back(), target) <= options.tolerance;
}

void Chain::follow(math::Vec2 head) {
    pushFromRoot(head);
}

void Chain::straighten(float angle) {
    const math::Vec2 direction = math::Vec2::fromAngle(angle);
    for (std::size_t index = 0; index < lengths.size(); ++index) {
        points[index + 1] = points[index] + direction * lengths[index];
    }
}

void Chain::setRoot(math::Vec2 value) noexcept {
    const math::Vec2 offset = value - points.front();
    for (math::Vec2& point : points) {
        point = point + offset;
    }
}

void Chain::setPoint(std::size_t index, math::Vec2 value) {
    points.at(index) = value;
}

void Chain::setMaxBend(float value) noexcept {
    maxBend = std::clamp(value, 0.0F, math::Math::kPi);
}

math::Vec2 Chain::directionOr(math::Vec2 value, math::Vec2 fallback) noexcept {
    if (!value.isZero()) {
        return value.getNormalized();
    }
    return fallback.isZero() ? math::Vec2{1.0F, 0.0F} : fallback.getNormalized();
}

math::Vec2 Chain::limitBend(math::Vec2 direction, math::Vec2 previous) const noexcept {
    const float bend = std::atan2(math::Vec2::cross(previous, direction), math::Vec2::dot(previous, direction));
    if (std::abs(bend) <= maxBend) {
        return direction;
    }
    return previous.rotated(std::copysign(maxBend, bend));
}

// Places the end on the target and every point before it at the length of its segment from the next one.
void Chain::pullToward(math::Vec2 target) {
    points.back() = target;
    for (std::size_t index = lengths.size(); index-- > 0;) {
        const math::Vec2 fallback = index + 2 < points.size() ? points[index + 1] - points[index + 2] : math::Vec2{-1.0F, 0.0F};
        points[index] = points[index + 1] + directionOr(points[index] - points[index + 1], fallback) * lengths[index];
    }
}

// Places the first point on the root and every point after it at the length of its segment from the one before, within the bend limit.
void Chain::pushFromRoot(math::Vec2 root) {
    points.front() = root;
    for (std::size_t index = 1; index < points.size(); ++index) {
        math::Vec2 direction;
        if (index == 1) {
            direction = directionOr(points[1] - points[0], math::Vec2{1.0F, 0.0F});
        } else {
            const math::Vec2 previous = (points[index - 1] - points[index - 2]) / lengths[index - 2];
            direction = limitBend(directionOr(points[index] - points[index - 1], previous), previous);
        }
        points[index] = points[index - 1] + direction * lengths[index - 1];
    }
}

// Mirrors every inner point that lies on the other side of the line from the root to the target than the pole, and pushes a point on the line toward the pole, so the joints bend toward it.
void Chain::bendTowardPole(math::Vec2 target, math::Vec2 pole) {
    const math::Vec2 root = points.front();
    const math::Vec2 axis = directionOr(target - root, points.back() - root);
    const float side = math::Vec2::cross(axis, pole - root);
    if (side == 0.0F) {
        return;
    }

    const math::Vec2 across = axis.getPerpendicular() * std::copysign(1.0F, side);
    for (std::size_t index = 1; index + 1 < points.size(); ++index) {
        const math::Vec2 offset = points[index] - root;
        const float distance = math::Vec2::cross(axis, offset) * std::copysign(1.0F, side);
        if (distance > 0.0F) {
            continue;
        }
        points[index] = distance == 0.0F ? points[index] + across * lengths[index - 1] * 0.5F : root + axis * (math::Vec2::dot(offset, axis) * 2.0F) - offset;
    }
}

} // namespace haylen::animation2d
