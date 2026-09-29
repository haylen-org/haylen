#include "haylen/2d/physics/Body.hpp"

#include <box2d/box2d.h>

#include <algorithm>
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

std::uint64_t Body::checkedId() const {
    if (!isValid()) {
        throw std::logic_error("The physics body was destroyed.");
    }
    return id;
}

bool Body::isValid() const noexcept {
    return world != nullptr && b2Body_IsValid(b2LoadBodyId(id));
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

float Body::getMass() const {
    return b2Body_GetMass(b2LoadBodyId(checkedId()));
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
    b2Body_SetLinearDamping(b2LoadBodyId(checkedId()), value);
}

float Body::getAngularDamping() const {
    return b2Body_GetAngularDamping(b2LoadBodyId(checkedId()));
}

void Body::setAngularDamping(float value) {
    b2Body_SetAngularDamping(b2LoadBodyId(checkedId()), value);
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
    if (points.size() < 4) {
        throw std::invalid_argument("A physics chain needs at least four points.");
    }

    const std::vector<b2Vec2> local = Box2DConverter::toLocalPoints(points, options, world->getPixelsPerMeter());
    b2SurfaceMaterial material = b2DefaultSurfaceMaterial();
    material.friction = options.friction;
    material.restitution = options.restitution;

    b2ChainDef def = b2DefaultChainDef();
    def.points = local.data();
    def.count = static_cast<int>(local.size());
    def.materials = &material;
    def.materialCount = 1;
    def.filter = Box2DConverter::toFilter(options.filter);
    def.isLoop = loop;
    def.enableSensorEvents = true;
    const b2ChainId chain = b2CreateChain(b2LoadBodyId(checkedId()), &def);

    std::vector<b2ShapeId> segments(static_cast<std::size_t>(b2Chain_GetSegmentCount(chain)));
    b2Chain_GetSegments(chain, segments.data(), static_cast<int>(segments.size()));
    std::vector<Shape> shapes;
    for (const b2ShapeId segment : segments) {
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
        world->countObjects();
    }
}

} // namespace haylen::physics2d
