#include "haylen/2d/physics/Grabber.hpp"

#include <box2d/box2d.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "2d/physics/Box2DConverter.hpp"
#include "haylen/2d/physics/World.hpp"

namespace haylen::physics2d {

Grabber::Grabber(World& owner, const Options& settings) : world(owner) {
    setPickRadius(settings.pickRadius);
    setStrength(settings.strength);
    setHertz(settings.hertz);
    setDampingRatio(settings.dampingRatio);
    options.filter = settings.filter;
    anchor = world.createBody({.type = Body::Type::Static});
}

Grabber::~Grabber() {
    anchor.destroy();
}

void Grabber::setPickRadius(float value) {
    if (!std::isfinite(value) || value < 0.0F) {
        throw std::invalid_argument("A grabber needs a finite pick radius of zero or more.");
    }
    options.pickRadius = value;
}

void Grabber::setStrength(float value) {
    options.strength = Box2DConverter::toPositive(value, "A grabber needs a positive strength, stiffness and damping ratio.");
    if (isHolding()) {
        joint.setMaxMotorForce(options.strength * getConnectedMass(held) * kStandardGravity * world.getPixelsPerMeter());
    }
}

void Grabber::setHertz(float value) {
    options.hertz = Box2DConverter::toPositive(value, "A grabber needs a positive strength, stiffness and damping ratio.");
    if (isHolding()) {
        joint.setHertz(options.hertz);
    }
}

void Grabber::setDampingRatio(float value) {
    options.dampingRatio = Box2DConverter::toPositive(value, "A grabber needs a positive strength, stiffness and damping ratio.");
    if (isHolding()) {
        joint.setDampingRatio(options.dampingRatio);
    }
}

float Grabber::getConnectedMass(const Body& body) const {
    std::vector<b2BodyId> pending{b2LoadBodyId(body.getId())};
    std::vector<std::uint64_t> seen{body.getId()};
    std::vector<b2JointId> joints;
    float mass = 0.0F;
    while (!pending.empty()) {
        const b2BodyId current = pending.back();
        pending.pop_back();
        mass += b2Body_GetMass(current);

        joints.resize(static_cast<std::size_t>(b2Body_GetJointCount(current)));
        b2Body_GetJoints(current, joints.data(), static_cast<int>(joints.size()));
        for (const b2JointId link : joints) {
            const b2BodyId first = b2Joint_GetBodyA(link);
            const b2BodyId other = B2_ID_EQUALS(first, current) ? b2Joint_GetBodyB(link) : first;
            const std::uint64_t id = b2StoreBodyId(other);
            if (b2Body_GetType(other) == b2_dynamicBody && std::ranges::find(seen, id) == seen.end()) {
                seen.push_back(id);
                pending.push_back(other);
            }
        }
    }
    return mass;
}

std::optional<Body> Grabber::grab(math::Vec2 point) {
    release();
    for (const Shape& shape : world.pick(point, options.pickRadius, options.filter)) {
        const b2ShapeId id = b2LoadShapeId(shape.getId());
        const Body body = shape.getBody();
        if (b2Shape_IsSensor(id) || body.getType() != Body::Type::Dynamic) {
            continue;
        }

        // A point outside the shape takes the body by the nearest point of the shape, so the pull never jumps.
        const float scale = world.getPixelsPerMeter();
        const b2Vec2 location = Box2DConverter::toMeters(point, scale);
        const math::Vec2 grip = b2Shape_TestPoint(id, location) ? point : Box2DConverter::toPixels(b2Shape_GetClosestPoint(id, location), scale);
        const float force = options.strength * getConnectedMass(body) * kStandardGravity * scale;
        joint = world.createJoint(Joint::Type::Mouse, anchor, body, {.anchorB = grip, .maxMotorForce = force, .hertz = options.hertz, .dampingRatio = options.dampingRatio});
        held = body;
        target = grip;
        const b2Vec2 local = b2Body_GetLocalPoint(b2LoadBodyId(body.getId()), Box2DConverter::toMeters(grip, scale));
        handle = {local.x, local.y};
        held.setAwake(true);
        return held;
    }
    return std::nullopt;
}

void Grabber::moveTo(math::Vec2 value) {
    target = value;
    if (isHolding()) {
        joint.setTarget(value);
    }
}

void Grabber::release() {
    joint.destroy();
    joint = {};
    held = {};
}

bool Grabber::isHolding() const {
    return joint.isValid();
}

std::optional<Body> Grabber::getBody() const {
    return isHolding() ? std::optional(held) : std::nullopt;
}

math::Vec2 Grabber::getHandle() const {
    if (!isHolding()) {
        return target;
    }
    return Box2DConverter::toPixels(b2Body_GetWorldPoint(b2LoadBodyId(held.getId()), {handle.x, handle.y}), world.getPixelsPerMeter());
}

float Grabber::getForce() const {
    return isHolding() ? joint.getMaxMotorForce() : 0.0F;
}

} // namespace haylen::physics2d
