#include "haylen/2d/physics/ForceField.hpp"

#include <box2d/box2d.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "2d/physics/Box2DConverter.hpp"
#include "haylen/2d/physics/Body.hpp"
#include "haylen/2d/physics/World.hpp"
#include "haylen/math/Geometry.hpp"
#include "haylen/math/Math.hpp"

namespace haylen::physics2d {

const std::array<std::pair<std::string_view, ForceField::Kind>, 4> ForceField::kKindNames{{{"radial", Kind::Radial}, {"directional", Kind::Directional}, {"vortex", Kind::Vortex}, {"buoyancy", Kind::Buoyancy}}};
const std::array<std::pair<std::string_view, ForceField::Falloff>, 3> ForceField::kFalloffNames{{{"none", Falloff::None}, {"linear", Falloff::Linear}, {"inverseSquare", Falloff::InverseSquare}}};

std::optional<ForceField::Kind> ForceField::kindFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kKindNames, name, &std::pair<std::string_view, Kind>::first);
    return found != kKindNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view ForceField::kindName(Kind value) noexcept {
    return std::ranges::find(kKindNames, value, &std::pair<std::string_view, Kind>::second)->first;
}

std::optional<ForceField::Falloff> ForceField::falloffFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kFalloffNames, name, &std::pair<std::string_view, Falloff>::first);
    return found != kFalloffNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view ForceField::falloffName(Falloff value) noexcept {
    return std::ranges::find(kFalloffNames, value, &std::pair<std::string_view, Falloff>::second)->first;
}

ForceField::ForceField(World& owner, const Options& settings) : world(owner), options(settings) {
    if (options.radius <= 0.0F && options.size.x > 0.0F && options.size.y > 0.0F) {
        const math::Vec2 half = options.size * 0.5F;
        area = {{-half.x, -half.y}, {half.x, -half.y}, {half.x, half.y}, {-half.x, half.y}};
    } else if (options.radius <= 0.0F) {
        area = options.points;
    }
    check();
    hook = world.addStepHook(World::StepPhase::Before, [this](float deltaSeconds) { update(deltaSeconds); });
    wake();
}

ForceField::~ForceField() {
    destroy();
}

void ForceField::destroy() {
    world.removeStepHook(hook);
    hook = 0;
    bodyCount = 0;
}

void ForceField::check() const {
    const bool shaped = options.radius > 0.0F || area.size() >= 3;
    const bool convex = options.kind != Kind::Buoyancy || (options.radius <= 0.0F && math::Geometry::isConvex(area));
    const bool drags = options.linearDrag >= 0.0F && options.angularDrag >= 0.0F && options.density >= 0.0F && options.minDistance >= 0.0F;
    if (!shaped || !convex || !drags || options.direction.isZero() || (options.falloff == Falloff::InverseSquare && options.minDistance <= 0.0F)) {
        throw std::invalid_argument("A force field needs a circle, a rectangle or a polygon of at least three points, convex for buoyancy, a direction that is not zero, drags, a density and a minimum distance of zero or more, and a minimum distance for an inverse square falloff.");
    }
}

void ForceField::setEnabled(bool value) {
    options.enabled = value;
    wake();
}

void ForceField::setStrength(float value) {
    options.strength = value;
    wake();
}

void ForceField::setPosition(math::Vec2 value) {
    options.position = value;
    wake();
}

void ForceField::setDirection(math::Vec2 value) {
    if (value.isZero()) {
        throw std::invalid_argument("A force field needs a direction that is not zero.");
    }
    options.direction = value;
    wake();
}

void ForceField::setFlow(math::Vec2 value) {
    options.flow = value;
    wake();
}

void ForceField::setDensity(float value) {
    if (!(value >= 0.0F)) {
        throw std::invalid_argument("A force field needs a density of zero or more.");
    }
    options.density = value;
    wake();
}

void ForceField::setLinearDrag(float value) {
    if (!(value >= 0.0F)) {
        throw std::invalid_argument("A force field needs drags of zero or more.");
    }
    options.linearDrag = value;
}

void ForceField::setAngularDrag(float value) {
    if (!(value >= 0.0F)) {
        throw std::invalid_argument("A force field needs drags of zero or more.");
    }
    options.angularDrag = value;
}

math::Rect ForceField::getBounds() const {
    if (options.radius > 0.0F) {
        return {options.position.x - options.radius, options.position.y - options.radius, options.radius * 2.0F, options.radius * 2.0F};
    }
    math::Rect bounds = math::Geometry::bounds(area);
    bounds.x += options.position.x;
    bounds.y += options.position.y;
    return bounds;
}

bool ForceField::contains(math::Vec2 point) const {
    const math::Vec2 local = point - options.position;
    if (options.radius > 0.0F) {
        return local.getLengthSquared() <= options.radius * options.radius;
    }
    return math::Geometry::contains(area, local);
}

float ForceField::fade(float distance) const noexcept {
    switch (options.falloff) {
    case Falloff::None:
        return 1.0F;
    case Falloff::Linear: {
        const math::Rect bounds = getBounds();
        const float reach = options.radius > 0.0F ? options.radius : std::max(bounds.width, bounds.height) * 0.5F;
        return std::max(0.0F, 1.0F - distance / reach);
    }
    case Falloff::InverseSquare:
        break;
    }
    const float ratio = options.minDistance / std::max(distance, options.minDistance);
    return ratio * ratio;
}

void ForceField::gather() {
    targets.clear();
    const float scale = world.getPixelsPerMeter();
    const math::Rect bounds = getBounds();
    const b2AABB box{Box2DConverter::toMeters(bounds.getMin(), scale), Box2DConverter::toMeters(bounds.getMax(), scale)};
    // clang-format off
    b2World_OverlapAABB(b2LoadWorldId(world.getHandle()), box, b2DefaultQueryFilter(), [](b2ShapeId shape, void* context) {
        auto& field = *static_cast<ForceField*>(context);
        const b2BodyId body = b2Shape_GetBody(shape);
        if (!b2Shape_IsSensor(shape) && b2Body_GetType(body) == b2_dynamicBody && Box2DConverter::collides(field.options.filter, b2Shape_GetFilter(shape))) {
            field.targets.push_back({b2StoreBodyId(body), b2StoreShapeId(shape)});
        }
        return true;
    }, this);
    // clang-format on
    std::ranges::sort(targets, [](const Target& lhs, const Target& rhs) { return lhs.body < rhs.body || (lhs.body == rhs.body && lhs.shape < rhs.shape); });
}

void ForceField::wake() {
    gather();
    for (const Target& target : targets) {
        b2Body_SetAwake(b2LoadBodyId(target.body), true);
    }
}

void ForceField::update(float) {
    bodyCount = 0;
    if (!options.enabled) {
        return;
    }
    gather();
    for (std::size_t index = 0; index < targets.size(); ++index) {
        const Body body(&world, targets[index].body);
        if (options.kind == Kind::Buoyancy) {
            floatShape(body, targets[index].shape);
        } else if (index == 0 || targets[index - 1].body != targets[index].body) {
            push(body);
        }
        if (index == 0 || targets[index - 1].body != targets[index].body) {
            ++bodyCount;
        }
    }
}

// Pushes a body whose center of mass lies in the area.
void ForceField::push(const Body& body) {
    const math::Vec2 center = body.getWorldCenter();
    if (!contains(center)) {
        return;
    }
    const math::Vec2 offset = center - options.position;
    const float distance = offset.getLength();
    math::Vec2 pull{};
    switch (options.kind) {
    case Kind::Radial:
        pull = distance > 0.0F ? offset * (-options.strength * fade(distance) / distance) : math::Vec2{};
        break;
    case Kind::Directional:
        pull = options.direction.getNormalized() * (options.strength * fade(distance));
        break;
    case Kind::Vortex:
        pull = distance > 0.0F ? math::Vec2{-offset.y, offset.x} * (options.strength * fade(distance) / distance) : math::Vec2{};
        break;
    case Kind::Buoyancy:
        break;
    }

    const float mass = body.getMass();
    math::Vec2 force = options.acceleration ? pull * mass : pull;
    force -= (body.getVelocity() - options.flow) * (options.linearDrag * mass);
    const float scale = world.getPixelsPerMeter();
    const b2BodyId id = b2LoadBodyId(body.getId());
    b2Body_ApplyForceToCenter(id, Box2DConverter::toMeters(force, scale), false);
    if (options.angularDrag > 0.0F) {
        b2Body_ApplyTorque(id, -options.angularDrag * b2Body_GetRotationalInertia(id) * b2Body_GetAngularVelocity(id), false);
    }
}

// Lifts a shape by the weight of the water it displaces, at the centroid of its part under the surface, and drags that part toward the flow.
void ForceField::floatShape(const Body& body, std::uint64_t shape) {
    const std::vector<math::Vec2> outline = outlineOf(shape);
    if (outline.size() < 3) {
        return;
    }
    const std::vector<math::Vec2> submerged = clip(outline);
    const float wet = submerged.size() < 3 ? 0.0F : std::abs(math::Geometry::signedArea(submerged));
    if (wet <= 0.0F) {
        return;
    }

    const float scale = world.getPixelsPerMeter();
    const float displaced = options.density * wet / (scale * scale);
    const math::Vec2 centroid = math::Geometry::centroid(submerged);
    math::Vec2 force = world.getGravity() * -displaced;
    force -= (body.getVelocityAt(centroid) - options.flow) * (options.linearDrag * displaced);
    const b2BodyId id = b2LoadBodyId(body.getId());
    b2Body_ApplyForce(id, Box2DConverter::toMeters(force, scale), Box2DConverter::toMeters(centroid, scale), false);
    if (options.angularDrag > 0.0F) {
        const float share = wet / std::abs(math::Geometry::signedArea(outline));
        b2Body_ApplyTorque(id, -options.angularDrag * share * b2Body_GetRotationalInertia(id) * b2Body_GetAngularVelocity(id), false);
    }
}

std::vector<math::Vec2> ForceField::outlineOf(std::uint64_t shape) const {
    const b2ShapeId id = b2LoadShapeId(shape);
    const b2Transform transform = b2Body_GetTransform(b2Shape_GetBody(id));
    const float scale = world.getPixelsPerMeter();
    std::vector<math::Vec2> points;
    // clang-format off
    const auto addArc = [&](b2Vec2 center, float radius, float start) {
        for (int step = 0; step <= kRoundSteps; ++step) {
            const float angle = start + math::Math::kPi * static_cast<float>(step) / static_cast<float>(kRoundSteps);
            points.push_back(Box2DConverter::toPixels(b2TransformPoint(transform, {center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius}), scale));
        }
    };
    // clang-format on
    switch (b2Shape_GetType(id)) {
    case b2_circleShape: {
        const b2Circle circle = b2Shape_GetCircle(id);
        addArc(circle.center, circle.radius, 0.0F);
        points.pop_back();
        addArc(circle.center, circle.radius, math::Math::kPi);
        points.pop_back();
        break;
    }
    case b2_capsuleShape: {
        const b2Capsule capsule = b2Shape_GetCapsule(id);
        const float axis = std::atan2(capsule.center2.y - capsule.center1.y, capsule.center2.x - capsule.center1.x);
        addArc(capsule.center2, capsule.radius, axis - math::Math::kHalfPi);
        addArc(capsule.center1, capsule.radius, axis + math::Math::kHalfPi);
        break;
    }
    case b2_polygonShape: {
        const b2Polygon polygon = b2Shape_GetPolygon(id);
        for (int index = 0; index < polygon.count; ++index) {
            points.push_back(Box2DConverter::toPixels(b2TransformPoint(transform, polygon.vertices[index]), scale));
        }
        break;
    }
    default:
        break;
    }
    return points;
}

// Sutherland and Hodgman clipping keeps the part of the polygon on the inner side of every edge of the convex area.
std::vector<math::Vec2> ForceField::clip(std::vector<math::Vec2> polygon) const {
    const float winding = math::Geometry::signedArea(area) >= 0.0F ? 1.0F : -1.0F;
    std::vector<math::Vec2> kept;
    for (std::size_t edge = 0; edge < area.size() && !polygon.empty(); ++edge) {
        const math::Vec2 from = area[edge] + options.position;
        const math::Vec2 to = area[(edge + 1) % area.size()] + options.position;
        const auto side = [&](math::Vec2 point) { return math::Vec2::cross(to - from, point - from) * winding; };
        kept.clear();
        for (std::size_t index = 0; index < polygon.size(); ++index) {
            const math::Vec2 current = polygon[index];
            const math::Vec2 next = polygon[(index + 1) % polygon.size()];
            const float currentSide = side(current);
            const float nextSide = side(next);
            if (currentSide >= 0.0F) {
                kept.push_back(current);
            }
            if ((currentSide >= 0.0F) != (nextSide >= 0.0F)) {
                kept.push_back(math::Vec2::lerp(current, next, currentSide / (currentSide - nextSide)));
            }
        }
        polygon.swap(kept);
    }
    return polygon;
}

} // namespace haylen::physics2d
