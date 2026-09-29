#include "haylen/2d/physics/Rope.hpp"

#include <algorithm>
#include <stdexcept>

#include "haylen/2d/physics/World.hpp"

namespace haylen::physics2d {

Rope Rope::create(World& world, const Options& options) {
    if (options.segments < 1 || (options.end - options.start).isZero() || options.thickness <= 0.0F) {
        throw std::invalid_argument("A rope needs at least one segment, two distinct ends and a positive thickness.");
    }

    // A rope that fails halfway, such as on a bad material or an end body of another world, leaves no bodies behind.
    Rope rope;
    try {
        rope.build(world, options);
    } catch (...) {
        rope.destroy();
        throw;
    }
    return rope;
}

void Rope::build(World& world, const Options& options) {
    const math::Vec2 span = options.end - options.start;
    const math::Vec2 direction = span.getNormalized();
    const float angle = span.getAngle();
    segmentLength = span.getLength() / static_cast<float>(options.segments);
    const float radius = options.thickness * 0.5F;
    const Shape::Options shape{.density = options.density, .friction = options.friction, .filter = options.filter};

    for (int index = 0; index < options.segments; ++index) {
        const math::Vec2 center = options.start + direction * (segmentLength * (static_cast<float>(index) + 0.5F));
        Body body = bodies.emplace_back(world.createBody({.position = center, .rotation = angle, .linearDamping = options.linearDamping, .angularDamping = options.angularDamping}));

        // Capsules shorter than their own thickness become circles.
        const float reach = segmentLength * 0.5F - radius;
        if (options.planks) {
            body.addBox({segmentLength, options.thickness}, shape);
        } else if (reach > 0.0F) {
            body.addCapsule({-reach, 0.0F}, {reach, 0.0F}, radius, shape);
        } else {
            body.addCircle(radius, shape);
        }

        if (bodies.size() > 1) {
            joints.push_back(world.createJoint(Joint::Type::Revolute, bodies[bodies.size() - 2], body, {.anchorA = center - direction * (segmentLength * 0.5F)}));
        }
    }

    // clang-format off
    const auto attach = [&](std::optional<Body> holder, bool pinned, math::Vec2 point, Body segment) {
        if (!holder && pinned) {
            holder = anchors.emplace_back(world.createBody({.type = Body::Type::Static, .position = point}));
        }
        if (holder) {
            joints.push_back(world.createJoint(Joint::Type::Revolute, *holder, segment, {.anchorA = point}));
        }
    };
    // clang-format on
    attach(options.startBody, options.pinStart, options.start, bodies.front());
    attach(options.endBody, options.pinEnd, options.end, bodies.back());
}

Rope Rope::createBridge(World& world, Options options) {
    options.planks = true;
    options.pinStart = true;
    options.pinEnd = true;
    return create(world, options);
}

bool Rope::isValid() const noexcept {
    return !bodies.empty() && std::all_of(bodies.begin(), bodies.end(), [](const Body& body) { return body.isValid(); });
}

std::vector<Rope::Segment> Rope::getSegments() const {
    std::vector<Segment> segments;
    segments.reserve(bodies.size());
    for (const Body& body : bodies) {
        segments.push_back({body.getPosition(), body.getRotation(), segmentLength});
    }
    return segments;
}

std::vector<math::Vec2> Rope::getPoints() const {
    std::vector<math::Vec2> points;
    points.reserve(bodies.size() + 1);
    for (const Body& body : bodies) {
        const math::Vec2 half = math::Vec2::fromAngle(body.getRotation(), segmentLength * 0.5F);
        if (points.empty()) {
            points.push_back(body.getPosition() - half);
        }
        points.push_back(body.getPosition() + half);
    }
    return points;
}

void Rope::destroy() {
    for (Body& body : bodies) {
        body.destroy();
    }
    for (Body& anchor : anchors) {
        anchor.destroy();
    }
    bodies.clear();
    anchors.clear();
    joints.clear();
}

} // namespace haylen::physics2d
