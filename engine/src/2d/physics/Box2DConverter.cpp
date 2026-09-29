#include "2d/physics/Box2DConverter.hpp"

#include <cmath>
#include <stdexcept>

namespace haylen::physics2d {

b2Vec2 Box2DConverter::toMeters(math::Vec2 value, float scale) noexcept {
    return {value.x / scale, value.y / scale};
}

math::Vec2 Box2DConverter::toPixels(b2Vec2 value, float scale) noexcept {
    return {value.x * scale, value.y * scale};
}

b2BodyType Box2DConverter::toBodyType(Body::Type type) noexcept {
    switch (type) {
    case Body::Type::Static:
        return b2_staticBody;
    case Body::Type::Kinematic:
        return b2_kinematicBody;
    case Body::Type::Dynamic:
        break;
    }
    return b2_dynamicBody;
}

b2Filter Box2DConverter::toFilter(const CollisionFilter& filter) noexcept {
    b2Filter result = b2DefaultFilter();
    result.categoryBits = filter.category;
    result.maskBits = filter.mask;
    result.groupIndex = filter.group;
    return result;
}

b2QueryFilter Box2DConverter::toQueryFilter(const CollisionFilter& filter) noexcept {
    return {.categoryBits = filter.category, .maskBits = filter.mask};
}

bool Box2DConverter::isFiniteAndNotNegative(float value) noexcept {
    return std::isfinite(value) && value >= 0.0F;
}

void Box2DConverter::checkShapeOptions(const Shape::Options& options) {
    if (!isFiniteAndNotNegative(options.density) || !isFiniteAndNotNegative(options.friction) || !isFiniteAndNotNegative(options.restitution)) {
        throw std::invalid_argument("A physics shape needs a finite density, friction and restitution of zero or more.");
    }
    if (options.oneWay && options.oneWay->isZero()) {
        throw std::invalid_argument("A one-way direction cannot be zero.");
    }
}

b2SurfaceMaterial Box2DConverter::toSurfaceMaterial(const Shape::Options& options) {
    checkShapeOptions(options);
    b2SurfaceMaterial material = b2DefaultSurfaceMaterial();
    material.friction = options.friction;
    material.restitution = options.restitution;
    return material;
}

b2ShapeDef Box2DConverter::toShapeDef(const Shape::Options& options) {
    b2ShapeDef def = b2DefaultShapeDef();
    def.material = toSurfaceMaterial(options);
    def.density = options.density;
    def.filter = toFilter(options.filter);
    def.isSensor = options.sensor;
    def.enableSensorEvents = true;
    def.enableContactEvents = true;
    def.enableHitEvents = true;
    return def;
}

float Box2DConverter::toDamping(float value) {
    if (!isFiniteAndNotNegative(value)) {
        throw std::invalid_argument("A physics body needs a finite damping of zero or more.");
    }
    return value;
}

std::vector<b2Vec2> Box2DConverter::toLocalPoints(std::span<const math::Vec2> points, const Shape::Options& options, float scale) {
    const float cosine = std::cos(options.rotation);
    const float sine = std::sin(options.rotation);
    std::vector<b2Vec2> result;
    result.reserve(points.size());
    for (const math::Vec2 point : points) {
        const math::Vec2 turned{point.x * cosine - point.y * sine, point.x * sine + point.y * cosine};
        result.push_back(toMeters(turned + options.offset, scale));
    }
    return result;
}

b2Polygon Box2DConverter::toHullPolygon(std::span<const b2Vec2> points) {
    const b2Hull hull = b2ComputeHull(points.data(), static_cast<int>(points.size()));
    if (hull.count == 0) {
        throw std::invalid_argument("A physics polygon needs at least three points that are not on one line.");
    }
    return b2MakePolygon(&hull, 0.0F);
}

} // namespace haylen::physics2d
