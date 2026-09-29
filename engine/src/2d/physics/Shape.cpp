#include "haylen/2d/physics/Shape.hpp"

#include <box2d/box2d.h>

#include <algorithm>
#include <iterator>
#include <stdexcept>

#include "2d/physics/Box2DConverter.hpp"
#include "haylen/2d/physics/Body.hpp"
#include "haylen/2d/physics/World.hpp"
#include "haylen/math/Math.hpp"

namespace haylen::physics2d {

const std::array<std::pair<std::string_view, Shape::Kind>, 5> Shape::kKindNames{{{"circle", Kind::Circle}, {"capsule", Kind::Capsule}, {"segment", Kind::Segment}, {"polygon", Kind::Polygon}, {"chainSegment", Kind::ChainSegment}}};

std::optional<Shape::Kind> Shape::kindFromName(std::string_view name) noexcept {
    for (const auto& [text, kind] : kKindNames) {
        if (text == name) {
            return kind;
        }
    }
    return std::nullopt;
}

std::string_view Shape::kindName(Kind value) noexcept {
    for (const auto& [text, kind] : kKindNames) {
        if (kind == value) {
            return text;
        }
    }
    return kKindNames.back().first;
}

std::uint64_t Shape::checkedId() const {
    if (!isValid()) {
        throw std::logic_error("The physics shape was destroyed.");
    }
    return id;
}

bool Shape::isValid() const noexcept {
    return world != nullptr && b2Shape_IsValid(b2LoadShapeId(id));
}

Body Shape::getBody() const {
    return {world, b2StoreBodyId(b2Shape_GetBody(b2LoadShapeId(checkedId())))};
}

bool Shape::isSensor() const {
    return b2Shape_IsSensor(b2LoadShapeId(checkedId()));
}

Shape::Kind Shape::getKind() const {
    switch (b2Shape_GetType(b2LoadShapeId(checkedId()))) {
    case b2_circleShape:
        return Kind::Circle;
    case b2_capsuleShape:
        return Kind::Capsule;
    case b2_segmentShape:
        return Kind::Segment;
    case b2_polygonShape:
        return Kind::Polygon;
    default:
        return Kind::ChainSegment;
    }
}

std::vector<math::Vec2> Shape::readPoints(bool inWorld) const {
    const b2ShapeId shape = b2LoadShapeId(checkedId());
    std::vector<b2Vec2> local;
    switch (b2Shape_GetType(shape)) {
    case b2_circleShape:
        local = {b2Shape_GetCircle(shape).center};
        break;
    case b2_capsuleShape: {
        const b2Capsule capsule = b2Shape_GetCapsule(shape);
        local = {capsule.center1, capsule.center2};
        break;
    }
    case b2_segmentShape: {
        const b2Segment segment = b2Shape_GetSegment(shape);
        local = {segment.point1, segment.point2};
        break;
    }
    case b2_polygonShape: {
        const b2Polygon polygon = b2Shape_GetPolygon(shape);
        local.assign(std::begin(polygon.vertices), std::begin(polygon.vertices) + polygon.count);
        break;
    }
    default: {
        const b2Segment segment = b2Shape_GetChainSegment(shape).segment;
        local = {segment.point1, segment.point2};
        break;
    }
    }

    const b2Transform transform = inWorld ? b2Body_GetTransform(b2Shape_GetBody(shape)) : b2Transform_identity;
    const float scale = world->getPixelsPerMeter();
    std::vector<math::Vec2> points;
    points.reserve(local.size());
    for (const b2Vec2 point : local) {
        points.push_back(Box2DConverter::toPixels(b2TransformPoint(transform, point), scale));
    }
    return points;
}

std::vector<math::Vec2> Shape::getPoints() const {
    return readPoints(false);
}

std::vector<math::Vec2> Shape::getWorldPoints() const {
    return readPoints(true);
}

float Shape::getRadius() const {
    const b2ShapeId shape = b2LoadShapeId(checkedId());
    switch (b2Shape_GetType(shape)) {
    case b2_circleShape:
        return b2Shape_GetCircle(shape).radius * world->getPixelsPerMeter();
    case b2_capsuleShape:
        return b2Shape_GetCapsule(shape).radius * world->getPixelsPerMeter();
    default:
        return 0.0F;
    }
}

Shape::Outline Shape::getOutline() const {
    std::vector<math::Vec2> points = getWorldPoints();
    const Kind kind = getKind();
    if (kind == Kind::Polygon) {
        return {.points = std::move(points), .closed = true};
    }
    if (kind == Kind::Segment || kind == Kind::ChainSegment) {
        return {.points = std::move(points)};
    }

    const float radius = getRadius();
    const int steps = std::clamp(static_cast<int>(radius * 0.5F), kMinimumTurnSteps, kMaximumTurnSteps);
    Outline outline{.closed = true};
    if (kind == Kind::Circle) {
        for (int step = 0; step < steps; ++step) {
            outline.points.push_back(points[0] + math::Vec2::fromAngle(math::Math::kTau * static_cast<float>(step) / static_cast<float>(steps), radius));
        }
        return outline;
    }

    // A capsule runs around its far end and back around its near end, and the closing of the outline draws its straight sides.
    const float axis = (points[1] - points[0]).getAngle();
    const int halfSteps = steps / 2;
    for (const auto& [center, start] : {std::pair{points[1], axis - math::Math::kHalfPi}, std::pair{points[0], axis + math::Math::kHalfPi}}) {
        for (int step = 0; step <= halfSteps; ++step) {
            outline.points.push_back(center + math::Vec2::fromAngle(start + math::Math::kPi * static_cast<float>(step) / static_cast<float>(halfSteps), radius));
        }
    }
    return outline;
}

CollisionFilter Shape::getFilter() const {
    const b2Filter filter = b2Shape_GetFilter(b2LoadShapeId(checkedId()));
    return {.category = filter.categoryBits, .mask = filter.maskBits, .group = filter.groupIndex};
}

void Shape::setFilter(const CollisionFilter& value) {
    b2Shape_SetFilter(b2LoadShapeId(checkedId()), Box2DConverter::toFilter(value));
}

math::Rect Shape::getBounds() const {
    const b2AABB box = b2Shape_GetAABB(b2LoadShapeId(checkedId()));
    const float scale = world->getPixelsPerMeter();
    return math::Rect::fromMinMax(Box2DConverter::toPixels(box.lowerBound, scale), Box2DConverter::toPixels(box.upperBound, scale));
}

void Shape::destroy() {
    const b2ShapeId shape = b2LoadShapeId(id);
    if (!isValid()) {
        return;
    }
    if (b2Shape_GetType(shape) == b2_chainSegmentShape) {
        throw std::logic_error("Chain segments are destroyed together with their body.");
    }
    b2DestroyShape(shape, true);
}

// Box2D moves bodies along the right perpendicular of the contact normal, which runs counter-clockwise on screen in the downward y of the engine, so the sign flips.
float Shape::getTangentSpeed() const {
    return -b2Shape_GetSurfaceMaterial(b2LoadShapeId(checkedId())).tangentSpeed * world->getPixelsPerMeter();
}

void Shape::setTangentSpeed(float value) {
    const b2ShapeId shape = b2LoadShapeId(checkedId());
    b2SurfaceMaterial material = b2Shape_GetSurfaceMaterial(shape);
    material.tangentSpeed = -value / world->getPixelsPerMeter();
    b2Shape_SetSurfaceMaterial(shape, material);
}

std::optional<math::Vec2> Shape::getOneWay() const {
    const auto found = world->oneWayShapes.find(checkedId());
    return found == world->oneWayShapes.end() ? std::nullopt : std::optional<math::Vec2>(found->second);
}

void Shape::setOneWay(std::optional<math::Vec2> value) {
    const b2ShapeId shape = b2LoadShapeId(checkedId());
    if (value && value->isZero()) {
        throw std::invalid_argument("A one-way direction cannot be zero.");
    }
    if (value) {
        world->oneWayShapes[id] = value->getNormalized();
    } else {
        world->oneWayShapes.erase(id);
    }
    b2Shape_EnablePreSolveEvents(shape, value.has_value());
}

} // namespace haylen::physics2d
