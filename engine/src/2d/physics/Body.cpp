#include "haylen/2d/physics/Body.hpp"

#include <box2d/box2d.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "2d/physics/Box2DConverter.hpp"
#include "haylen/2d/physics/World.hpp"
#include "haylen/math/Geometry.hpp"
#include "haylen/math/Polygon.hpp"

namespace haylen::physics2d {

const std::array<std::pair<std::string_view, Body::Type>, 3> Body::kTypeNames{{{"static", Type::Static}, {"kinematic", Type::Kinematic}, {"dynamic", Type::Dynamic}}};

std::optional<Body::Type> Body::typeFromName(std::string_view name) noexcept {
    for (const auto& [text, type] : kTypeNames) {
        if (text == name) {
            return type;
        }
    }
    return std::nullopt;
}

std::string_view Body::typeName(Type value) noexcept {
    for (const auto& [text, type] : kTypeNames) {
        if (type == value) {
            return text;
        }
    }
    return kTypeNames.back().first;
}

Body::Body(World* owner, std::uint64_t handle) noexcept : world(owner), worldHandle(owner != nullptr ? owner->getHandle() : 0), id(handle) {}

std::uint64_t Body::checkedId() const {
    if (world != nullptr && !b2World_IsValid(b2LoadWorldId(worldHandle))) {
        throw std::logic_error("The physics world of the body was destroyed.");
    }
    if (!isValid()) {
        throw std::logic_error("The physics body was destroyed.");
    }
    return id;
}

bool Body::isValid() const noexcept {
    return world != nullptr && b2World_IsValid(b2LoadWorldId(worldHandle)) && b2Body_IsValid(b2LoadBodyId(id));
}

Body::Type Body::getType() const {
    switch (b2Body_GetType(b2LoadBodyId(checkedId()))) {
    case b2_staticBody:
        return Type::Static;
    case b2_kinematicBody:
        return Type::Kinematic;
    default:
        return Type::Dynamic;
    }
}

void Body::setType(Type value) {
    b2Body_SetType(b2LoadBodyId(checkedId()), Box2DConverter::toBodyType(value));
}

math::Vec2 Body::getPosition() const {
    return Box2DConverter::toPixels(b2Body_GetPosition(b2LoadBodyId(checkedId())), world->getPixelsPerMeter());
}

float Body::getRotation() const {
    return b2Rot_GetAngle(b2Body_GetRotation(b2LoadBodyId(checkedId())));
}

void Body::setTransform(math::Vec2 position, float rotation) {
    b2Body_SetTransform(b2LoadBodyId(checkedId()), Box2DConverter::toMeters(position, world->getPixelsPerMeter()), b2MakeRot(rotation));
}

void Body::moveTo(math::Vec2 position, float rotation, float seconds) {
    const b2BodyId body = b2LoadBodyId(checkedId());
    if (!std::isfinite(seconds) || seconds <= 0.0F) {
        throw std::invalid_argument("A physics body moves to a target over a positive time.");
    }

    // The velocities move the center of mass to where the target puts it, and a target the body already holds stops it.
    const b2Transform current = b2Body_GetTransform(body);
    const b2Transform target{Box2DConverter::toMeters(position, world->getPixelsPerMeter()), b2MakeRot(rotation)};
    const b2Vec2 center = b2Body_GetLocalCenterOfMass(body);
    b2Body_SetLinearVelocity(body, b2MulSV(1.0F / seconds, b2Sub(b2TransformPoint(target, center), b2TransformPoint(current, center))));
    if (!b2Body_IsFixedRotation(body)) {
        b2Body_SetAngularVelocity(body, b2RelativeAngle(target.q, current.q) / seconds);
    }
}

math::Vec2 Body::getVelocity() const {
    return Box2DConverter::toPixels(b2Body_GetLinearVelocity(b2LoadBodyId(checkedId())), world->getPixelsPerMeter());
}

void Body::setVelocity(math::Vec2 value) {
    b2Body_SetLinearVelocity(b2LoadBodyId(checkedId()), Box2DConverter::toMeters(value, world->getPixelsPerMeter()));
}

float Body::getAngularVelocity() const {
    return b2Body_GetAngularVelocity(b2LoadBodyId(checkedId()));
}

void Body::setAngularVelocity(float value) {
    b2Body_SetAngularVelocity(b2LoadBodyId(checkedId()), value);
}

math::Vec2 Body::getVelocityAt(math::Vec2 point) const {
    const float scale = world->getPixelsPerMeter();
    return Box2DConverter::toPixels(b2Body_GetWorldPointVelocity(b2LoadBodyId(checkedId()), Box2DConverter::toMeters(point, scale)), scale);
}

Body::MassData Body::getMassData() const {
    const b2MassData data = b2Body_GetMassData(b2LoadBodyId(checkedId()));
    const float scale = world->getPixelsPerMeter();
    return {.mass = data.mass, .center = Box2DConverter::toPixels(data.center, scale), .inertia = data.rotationalInertia * scale * scale};
}

void Body::setMassData(const MassData& value) {
    const b2BodyId body = b2LoadBodyId(checkedId());
    if (!std::isfinite(value.mass) || value.mass < 0.0F || !std::isfinite(value.inertia) || value.inertia < 0.0F || !std::isfinite(value.center.x) || !std::isfinite(value.center.y)) {
        throw std::invalid_argument("A physics body needs a finite mass, center of mass and inertia of zero or more.");
    }
    const float scale = world->getPixelsPerMeter();
    b2Body_SetMassData(body, {.mass = value.mass, .center = Box2DConverter::toMeters(value.center, scale), .rotationalInertia = value.inertia / (scale * scale)});
}

void Body::resetMassData() {
    b2Body_ApplyMassFromShapes(b2LoadBodyId(checkedId()));
}

float Body::getMass() const {
    return b2Body_GetMass(b2LoadBodyId(checkedId()));
}

void Body::setMass(float value) {
    MassData data = getMassData();
    data.inertia = data.mass > 0.0F ? data.inertia * value / data.mass : data.inertia;
    data.mass = value;
    setMassData(data);
}

math::Vec2 Body::getCenterOfMass() const {
    return getMassData().center;
}

void Body::setCenterOfMass(math::Vec2 value) {
    MassData data = getMassData();
    data.center = value;
    setMassData(data);
}

float Body::getInertia() const {
    return getMassData().inertia;
}

void Body::setInertia(float value) {
    MassData data = getMassData();
    data.inertia = value;
    setMassData(data);
}

math::Vec2 Body::getWorldCenter() const {
    return Box2DConverter::toPixels(b2Body_GetWorldCenterOfMass(b2LoadBodyId(checkedId())), world->getPixelsPerMeter());
}

void Body::applyForce(math::Vec2 force, std::optional<math::Vec2> point) {
    const float scale = world->getPixelsPerMeter();
    const b2BodyId body = b2LoadBodyId(checkedId());
    if (point) {
        b2Body_ApplyForce(body, Box2DConverter::toMeters(force, scale), Box2DConverter::toMeters(*point, scale), true);
        return;
    }
    b2Body_ApplyForceToCenter(body, Box2DConverter::toMeters(force, scale), true);
}

void Body::applyImpulse(math::Vec2 impulse, std::optional<math::Vec2> point) {
    const float scale = world->getPixelsPerMeter();
    const b2BodyId body = b2LoadBodyId(checkedId());
    if (point) {
        b2Body_ApplyLinearImpulse(body, Box2DConverter::toMeters(impulse, scale), Box2DConverter::toMeters(*point, scale), true);
        return;
    }
    b2Body_ApplyLinearImpulseToCenter(body, Box2DConverter::toMeters(impulse, scale), true);
}

void Body::applyTorque(float torque) {
    const float scale = world->getPixelsPerMeter();
    b2Body_ApplyTorque(b2LoadBodyId(checkedId()), torque / (scale * scale), true);
}

void Body::applyAngularImpulse(float impulse) {
    const float scale = world->getPixelsPerMeter();
    b2Body_ApplyAngularImpulse(b2LoadBodyId(checkedId()), impulse / (scale * scale), true);
}

float Body::getLinearDamping() const {
    return b2Body_GetLinearDamping(b2LoadBodyId(checkedId()));
}

void Body::setLinearDamping(float value) {
    b2Body_SetLinearDamping(b2LoadBodyId(checkedId()), Box2DConverter::toDamping(value));
}

float Body::getAngularDamping() const {
    return b2Body_GetAngularDamping(b2LoadBodyId(checkedId()));
}

void Body::setAngularDamping(float value) {
    b2Body_SetAngularDamping(b2LoadBodyId(checkedId()), Box2DConverter::toDamping(value));
}

float Body::getGravityScale() const {
    return b2Body_GetGravityScale(b2LoadBodyId(checkedId()));
}

void Body::setGravityScale(float value) {
    b2Body_SetGravityScale(b2LoadBodyId(checkedId()), value);
}

bool Body::isFixedRotation() const {
    return b2Body_IsFixedRotation(b2LoadBodyId(checkedId()));
}

void Body::setFixedRotation(bool value) {
    b2Body_SetFixedRotation(b2LoadBodyId(checkedId()), value);
}

bool Body::isBullet() const {
    return b2Body_IsBullet(b2LoadBodyId(checkedId()));
}

void Body::setBullet(bool value) {
    b2Body_SetBullet(b2LoadBodyId(checkedId()), value);
}

bool Body::isSleepEnabled() const {
    return b2Body_IsSleepEnabled(b2LoadBodyId(checkedId()));
}

void Body::setSleepEnabled(bool value) {
    b2Body_EnableSleep(b2LoadBodyId(checkedId()), value);
}

float Body::getSleepThreshold() const {
    return b2Body_GetSleepThreshold(b2LoadBodyId(checkedId())) * world->getPixelsPerMeter();
}

void Body::setSleepThreshold(float value) {
    b2Body_SetSleepThreshold(b2LoadBodyId(checkedId()), Box2DConverter::toSpeed(value, world->getPixelsPerMeter()));
}

void Body::dropThrough(float seconds) {
    const std::uint64_t checked = checkedId();
    if (!std::isfinite(seconds) || seconds < 0.0F) {
        throw std::invalid_argument("A physics body drops through one-way platforms for a finite time of zero or more.");
    }
    world->droppingBodies[checked] = seconds;
    b2Body_SetAwake(b2LoadBodyId(checked), true);
}

bool Body::isAwake() const {
    return b2Body_IsAwake(b2LoadBodyId(checkedId()));
}

void Body::setAwake(bool value) {
    b2Body_SetAwake(b2LoadBodyId(checkedId()), value);
}

bool Body::isEnabled() const {
    return b2Body_IsEnabled(b2LoadBodyId(checkedId()));
}

void Body::setEnabled(bool value) {
    if (value) {
        b2Body_Enable(b2LoadBodyId(checkedId()));
        return;
    }
    b2Body_Disable(b2LoadBodyId(checkedId()));
}

Shape Body::addBox(math::Vec2 size, const Shape::Options& options) {
    if (size.x <= 0.0F || size.y <= 0.0F) {
        throw std::invalid_argument("A physics box needs a positive size.");
    }
    const float scale = world->getPixelsPerMeter();
    const b2Polygon box = b2MakeOffsetBox(size.x * 0.5F / scale, size.y * 0.5F / scale, Box2DConverter::toMeters(options.offset, scale), b2MakeRot(options.rotation));
    const b2ShapeDef def = Box2DConverter::toShapeDef(options);
    return finishShape(b2StoreShapeId(b2CreatePolygonShape(b2LoadBodyId(checkedId()), &def, &box)), options);
}

Shape Body::addCircle(float radius, const Shape::Options& options) {
    if (radius <= 0.0F) {
        throw std::invalid_argument("A physics circle needs a positive radius.");
    }
    const float scale = world->getPixelsPerMeter();
    const b2Circle circle{.center = Box2DConverter::toMeters(options.offset, scale), .radius = radius / scale};
    const b2ShapeDef def = Box2DConverter::toShapeDef(options);
    return finishShape(b2StoreShapeId(b2CreateCircleShape(b2LoadBodyId(checkedId()), &def, &circle)), options);
}

Shape Body::addCapsule(math::Vec2 first, math::Vec2 second, float radius, const Shape::Options& options) {
    if (radius <= 0.0F) {
        throw std::invalid_argument("A physics capsule needs a positive radius.");
    }
    const std::array<math::Vec2, 2> ends{first, second};
    const std::vector<b2Vec2> points = Box2DConverter::toLocalPoints(ends, options, world->getPixelsPerMeter());
    const b2Capsule capsule{.center1 = points[0], .center2 = points[1], .radius = radius / world->getPixelsPerMeter()};
    const b2ShapeDef def = Box2DConverter::toShapeDef(options);
    return finishShape(b2StoreShapeId(b2CreateCapsuleShape(b2LoadBodyId(checkedId()), &def, &capsule)), options);
}

Shape Body::addSegment(math::Vec2 first, math::Vec2 second, const Shape::Options& options) {
    const std::array<math::Vec2, 2> ends{first, second};
    const std::vector<b2Vec2> points = Box2DConverter::toLocalPoints(ends, options, world->getPixelsPerMeter());
    if (!(b2Distance(points[0], points[1]) > Box2DConverter::kLinearSlop)) {
        throw std::invalid_argument("A physics segment needs ends more than 0.005 meters apart.");
    }
    const b2Segment segment{.point1 = points[0], .point2 = points[1]};
    const b2ShapeDef def = Box2DConverter::toShapeDef(options);
    return finishShape(b2StoreShapeId(b2CreateSegmentShape(b2LoadBodyId(checkedId()), &def, &segment)), options);
}

std::vector<Shape> Body::addPolygon(std::span<const math::Vec2> points, const Shape::Options& options) {
    if (points.size() < 3) {
        throw std::invalid_argument("A physics polygon needs at least three points.");
    }

    const std::vector<b2Vec2> local = Box2DConverter::toLocalPoints(points, options, world->getPixelsPerMeter());
    const b2ShapeDef def = Box2DConverter::toShapeDef(options);
    const b2BodyId body = b2LoadBodyId(checkedId());
    std::vector<Shape> shapes;
    if (points.size() <= B2_MAX_POLYGON_VERTICES && math::Geometry::isConvex(points)) {
        const b2Polygon polygon = Box2DConverter::toHullPolygon(local);
        shapes.push_back(finishShape(b2StoreShapeId(b2CreatePolygonShape(body, &def, &polygon)), options));
        return shapes;
    }

    const std::vector<std::vector<math::Vec2>> outline{{points.begin(), points.end()}};
    for (const std::vector<math::Vec2>& piece : math::Polygon::decompose(outline, B2_MAX_POLYGON_VERTICES)) {
        const std::vector<b2Vec2> corners = Box2DConverter::toLocalPoints(piece, options, world->getPixelsPerMeter());
        const b2Hull hull = b2ComputeHull(corners.data(), static_cast<int>(corners.size()));
        // Pieces thinner than the collision margin of Box2D add nothing to the collision.
        if (hull.count == 0) {
            continue;
        }
        const b2Polygon polygon = b2MakePolygon(&hull, 0.0F);
        shapes.push_back(finishShape(b2StoreShapeId(b2CreatePolygonShape(body, &def, &polygon)), options));
    }
    if (shapes.empty()) {
        throw std::invalid_argument("A physics polygon needs a non-degenerate outline.");
    }
    return shapes;
}

std::vector<Shape> Body::addChain(std::span<const math::Vec2> points, bool loop, const Shape::Options& options) {
    if (points.size() < (loop ? 4U : 2U)) {
        throw std::invalid_argument("A physics chain needs at least four points for a loop and two for an open chain.");
    }

    // Box2D keeps the first and last points of an open chain to smooth the contacts at its ends, so the world adds points that continue the first and last segments, and every segment listed collides.
    std::vector<math::Vec2> listed(points.begin(), points.end());
    if (!loop) {
        listed.insert(listed.begin(), points[0] * 2.0F - points[1]);
        listed.push_back(points[points.size() - 1] * 2.0F - points[points.size() - 2]);
    }
    const std::vector<b2Vec2> local = Box2DConverter::toLocalPoints(listed, options, world->getPixelsPerMeter());
    const b2SurfaceMaterial material = Box2DConverter::toSurfaceMaterial(options);

    b2ChainDef def = b2DefaultChainDef();
    def.points = local.data();
    def.count = static_cast<int>(local.size());
    def.materials = &material;
    def.materialCount = 1;
    def.filter = Box2DConverter::toFilter(options.filter);
    def.isLoop = loop;
    def.enableSensorEvents = options.sensorEvents;
    const b2ChainId chain = b2CreateChain(b2LoadBodyId(checkedId()), &def);

    std::vector<b2ShapeId> segments(static_cast<std::size_t>(b2Chain_GetSegmentCount(chain)));
    b2Chain_GetSegments(chain, segments.data(), static_cast<int>(segments.size()));
    std::vector<Shape> shapes;
    for (const b2ShapeId segment : segments) {
        b2Shape_EnableContactEvents(segment, options.contactEvents);
        b2Shape_EnableHitEvents(segment, options.hitEvents);
        shapes.push_back(finishShape(b2StoreShapeId(segment), options));
    }
    return shapes;
}

std::vector<Shape> Body::getShapes() const {
    const b2BodyId body = b2LoadBodyId(checkedId());
    std::vector<b2ShapeId> ids(static_cast<std::size_t>(b2Body_GetShapeCount(body)));
    b2Body_GetShapes(body, ids.data(), static_cast<int>(ids.size()));
    std::vector<Shape> result;
    for (const b2ShapeId shape : ids) {
        result.emplace_back(world, b2StoreShapeId(shape));
    }
    return result;
}

std::vector<Body::Contact> Body::getContacts() const {
    const b2BodyId body = b2LoadBodyId(checkedId());
    std::vector<b2ContactData> data(static_cast<std::size_t>(b2Body_GetContactCapacity(body)));
    data.resize(static_cast<std::size_t>(b2Body_GetContactData(body, data.data(), static_cast<int>(data.size()))));

    const float scale = world->getPixelsPerMeter();
    std::vector<Contact> contacts;
    for (const b2ContactData& contact : data) {
        if (contact.manifold.pointCount == 0) {
            continue;
        }
        // The manifold normal points from the first shape to the second, and the contact reads from the shape of this body.
        const bool first = B2_ID_EQUALS(b2Shape_GetBody(contact.shapeIdA), body);
        float impulse = 0.0F;
        for (int index = 0; index < contact.manifold.pointCount; ++index) {
            impulse += contact.manifold.points[index].totalNormalImpulse;
        }
        const math::Vec2 normal{contact.manifold.normal.x, contact.manifold.normal.y};
        contacts.push_back({
            .shape = {world, b2StoreShapeId(first ? contact.shapeIdA : contact.shapeIdB)},
            .other = {world, b2StoreShapeId(first ? contact.shapeIdB : contact.shapeIdA)},
            .point = Box2DConverter::toPixels(contact.manifold.points[0].point, scale),
            .normal = first ? normal : -normal,
            .impulse = impulse * scale,
        });
    }
    return contacts;
}

std::vector<Shape::Outline> Body::getOutlines() const {
    std::vector<Shape::Outline> outlines;
    std::vector<std::uint64_t> chains;
    for (const Shape& shape : getShapes()) {
        if (shape.getKind() != Shape::Kind::ChainSegment) {
            outlines.push_back(shape.getOutline());
            continue;
        }
        const std::uint64_t chain = b2StoreChainId(b2Shape_GetParentChain(b2LoadShapeId(shape.getId())));
        if (std::ranges::find(chains, chain) == chains.end()) {
            chains.push_back(chain);
            outlines.push_back(getChainOutline(chain));
        }
    }
    return outlines;
}

// The segments of a chain follow each other, and those of a loop end where the first one starts.
Shape::Outline Body::getChainOutline(std::uint64_t chainId) const {
    const b2ChainId chain = b2LoadChainId(chainId);
    std::vector<b2ShapeId> segments(static_cast<std::size_t>(b2Chain_GetSegmentCount(chain)));
    b2Chain_GetSegments(chain, segments.data(), static_cast<int>(segments.size()));

    Shape::Outline outline;
    for (const b2ShapeId segment : segments) {
        const std::vector<math::Vec2> ends = Shape(world, b2StoreShapeId(segment)).getWorldPoints();
        if (outline.points.empty()) {
            outline.points.push_back(ends[0]);
        }
        outline.points.push_back(ends[1]);
    }
    if (outline.points.size() > 2 && outline.points.front() == outline.points.back()) {
        outline.points.pop_back();
        outline.closed = true;
    }
    return outline;
}

Shape Body::finishShape(std::uint64_t shapeId, const Shape::Options& options) {
    Shape shape(world, shapeId);
    if (options.tangentSpeed != 0.0F) {
        shape.setTangentSpeed(options.tangentSpeed);
    }
    if (options.oneWay) {
        shape.setOneWay(options.oneWay);
    }
    return shape;
}

void Body::destroy() {
    if (isValid()) {
        b2DestroyBody(b2LoadBodyId(id));
        world->forgetBody(id);
        world->countObjects();
    }
}

} // namespace haylen::physics2d
