#include "haylen/2d/physics/Mover.hpp"

#include <box2d/box2d.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <optional>
#include <span>
#include <stdexcept>

#include "2d/physics/Box2DConverter.hpp"
#include "haylen/2d/physics/World.hpp"
#include "haylen/math/Math.hpp"

namespace haylen::physics2d {

// The Box2D side of the mover: its capsule, the shapes it ignores and the callbacks of its queries.
class Mover::Query final {
  public:
    // The capsule that collides runs from the top of the mover down to the step height above its feet.
    [[nodiscard]] static b2Capsule capsule(const Mover& mover, math::Vec2 center) noexcept {
        const auto& options = mover.options;
        const b2Vec2 top = Box2DConverter::toMeters(center + math::Vec2{0.0F, -options.height * 0.5F + options.radius}, mover.scale);
        const b2Vec2 bottom = Box2DConverter::toMeters(center + math::Vec2{0.0F, options.height * 0.5F - options.stepHeight - options.radius}, mover.scale);
        return {.center1 = top, .center2 = bottom, .radius = options.radius / mover.scale};
    }

    // The mover passes through sensors and its own body.
    [[nodiscard]] static bool ignores(const Mover& mover, b2ShapeId shape) noexcept {
        return b2Shape_IsSensor(shape) || B2_ID_EQUALS(b2Shape_GetBody(shape), b2LoadBodyId(mover.body.getId()));
    }

    // A one-way platform holds the mover only on its open side: a surface whose normal leans toward that side, which the mover moves into or touches without being inside the platform. A mover that drops through platforms passes them all.
    [[nodiscard]] static bool passesOneWay(const Mover& mover, b2ShapeId shape, math::Vec2 normal, bool holding) {
        const std::optional<math::Vec2> local = Shape(&mover.world, b2StoreShapeId(shape)).getOneWay();
        if (!local) {
            return false;
        }
        const b2Vec2 side = b2RotateVector(b2Body_GetRotation(b2Shape_GetBody(shape)), {local->x, local->y});
        return !holding || mover.world.isDroppingThrough(mover.body) || math::Vec2::dot(normal, {side.x, side.y}) < World::kOneWayThreshold;
    }

    [[nodiscard]] static bool movesInto(math::Vec2 motion, math::Vec2 normal) noexcept {
        return math::Vec2::dot(motion, normal) < 0.0F;
    }

    static bool addPlane(b2ShapeId shape, const b2PlaneResult* result, void* context) {
        auto& mover = *static_cast<Mover*>(context);
        const math::Vec2 normal{result->plane.normal.x, result->plane.normal.y};
        if (mover.planeCount < kPlaneCapacity && !ignores(mover, shape) && !passesOneWay(mover, shape, normal, result->plane.offset <= World::kOneWayDepth)) {
            mover.planes[mover.planeCount++] = {.normal = normal, .offset = result->plane.offset, .shape = b2StoreShapeId(shape)};
        }
        return true;
    }

    struct Search {
        const Mover* mover = nullptr;
        math::Vec2 motion{};
        Probe probe;
    };

    // Shapes the capsule already touches are what the planes handle, so the sweep looks past them.
    static float addHit(b2ShapeId shape, b2Vec2 point, b2Vec2 normal, float fraction, void* context) {
        auto& search = *static_cast<Search*>(context);
        if (fraction <= 0.0F || ignores(*search.mover, shape) || passesOneWay(*search.mover, shape, {normal.x, normal.y}, movesInto(search.motion, {normal.x, normal.y}))) {
            return -1.0F;
        }
        search.probe = {.hit = true, .fraction = fraction, .normal = {normal.x, normal.y}, .point = {point.x, point.y}, .shape = b2StoreShapeId(shape)};
        return fraction;
    }

    [[nodiscard]] static b2Vec2 meters(math::Vec2 value) noexcept {
        return {value.x, value.y};
    }
};

Mover::Mover(World& owner, const Options& settings) : world(owner), options(settings), position(settings.position), scale(owner.getPixelsPerMeter()) {
    if (!(options.radius / scale > 2.0F * Box2DConverter::kLinearSlop) || !(options.stepHeight >= 0.0F) || !(options.height - options.stepHeight >= options.radius * 2.0F)) {
        throw std::invalid_argument("A mover needs a radius of more than 0.01 meters, a step height of zero or more and a height that leaves twice the radius above the step height.");
    }
    if (!(options.pushForce >= 0.0F)) {
        throw std::invalid_argument("A mover needs a push force of zero or more.");
    }
    setMaxSlope(options.maxSlope);
    setSnapDistance(options.snapDistance);

    body = world.createBody({.type = Body::Type::Kinematic, .position = position, .fixedRotation = true});
    const float top = -options.height * 0.5F + options.radius;
    const float bottom = options.height * 0.5F - options.stepHeight - options.radius;
    body.addCapsule({0.0F, top}, {0.0F, bottom}, options.radius, {.filter = options.filter, .contactEvents = false, .hitEvents = false});
    stand(false);
}

Mover::~Mover() {
    destroy();
}

void Mover::destroy() {
    body.destroy();
}

void Mover::setMaxSlope(float value) {
    if (!(value >= 0.0F && value < math::Math::kHalfPi)) {
        throw std::invalid_argument("A mover needs a slope limit of zero or more and below half pi radians.");
    }
    options.maxSlope = value;
}

void Mover::setSnapDistance(float value) {
    if (!std::isfinite(value) || value < 0.0F) {
        throw std::invalid_argument("A mover needs a finite snap distance of zero or more.");
    }
    options.snapDistance = value;
}

// Ground faces up, which is negative y on screen, at most the slope limit away from straight up.
bool Mover::isWalkable(math::Vec2 normal) const noexcept {
    return -normal.y >= std::cos(options.maxSlope) - 1e-4F;
}

// Gathers the surfaces the capsule touches. On the ground, surfaces too steep to stand on act as walls, so walking never climbs them.
void Mover::gatherPlanes(bool flattenSteep) {
    planeCount = 0;
    const b2Capsule capsule = Query::capsule(*this, position);
    b2World_CollideMover(b2LoadWorldId(world.getHandle()), &capsule, Box2DConverter::toQueryFilter(options.filter), &Query::addPlane, this);
    if (!flattenSteep) {
        return;
    }
    for (Plane& plane : std::span(planes.data(), planeCount)) {
        if (!isWalkable(plane.normal) && plane.normal.y < 0.0F && plane.normal.x != 0.0F) {
            plane.normal = {std::copysign(1.0F, plane.normal.x), 0.0F};
        }
    }
}

Mover::Probe Mover::cast(math::Vec2 from, math::Vec2 translation) const {
    Query::Search search{.mover = this, .motion = translation};
    const b2Capsule capsule = Query::capsule(*this, from);
    const b2ShapeProxy proxy = b2MakeProxy(&capsule.center1, 2, capsule.radius);
    b2World_CastShape(b2LoadWorldId(world.getHandle()), &proxy, Box2DConverter::toMeters(translation, scale), Box2DConverter::toQueryFilter(options.filter), &Query::addHit, &search);
    return search.probe;
}

// On a slope the round bottom of the capsule stands farther above the ground under its center, so the ray reaches the steepest walkable ground too.
Mover::Probe Mover::castGround(float reach) const {
    Query::Search search{.mover = this, .motion = {0.0F, 1.0F}};
    const math::Vec2 origin = position + math::Vec2{0.0F, options.height * 0.5F - options.stepHeight - options.radius};
    const float length = options.stepHeight + options.radius / std::cos(options.maxSlope) + reach;
    b2World_CastRay(b2LoadWorldId(world.getHandle()), Box2DConverter::toMeters(origin, scale), {0.0F, length / scale}, Box2DConverter::toQueryFilter(options.filter), &Query::addHit, &search);
    return search.probe;
}

// Solves the planes for what is left of the delta and sweeps the capsule as far as it goes, a few times.
math::Vec2 Mover::slide(math::Vec2 delta, bool flattenSteep) {
    const math::Vec2 start = position;
    const math::Vec2 target = position + delta;
    for (int iteration = 0; iteration < kIterations; ++iteration) {
        gatherPlanes(flattenSteep);
        std::array<b2CollisionPlane, kPlaneCapacity> solverPlanes{};
        for (std::size_t index = 0; index < planeCount; ++index) {
            solverPlanes[index] = {.plane = {Query::meters(planes[index].normal), planes[index].offset}, .pushLimit = FLT_MAX, .clipVelocity = true};
        }
        const b2PlaneSolverResult solved = b2SolvePlanes(Box2DConverter::toMeters(target - position, scale), solverPlanes.data(), static_cast<int>(planeCount));
        const math::Vec2 translation = Box2DConverter::toPixels(solved.translation, scale);
        const math::Vec2 step = translation * cast(position, translation).fraction;
        position += step;
        if (step.getLength() < kTolerance * scale) {
            break;
        }
    }
    return position - start;
}

// The ray finds ground from inside the bottom of the capsule down to the feet, or past them by the snap distance, and the mover stands on what it finds, with its round bottom touching a slope and its feet on flat ground, rising onto steps without going through a ceiling.
void Mover::stand(bool snap) {
    const float reach = (snap ? options.snapDistance : 0.0F) + kSkin * 2.0F * scale;
    const Probe ground = castGround(reach);
    grounded = ground.hit && isWalkable(ground.normal);
    if (grounded) {
        const float bottom = position.y + options.height * 0.5F - options.stepHeight - options.radius;
        const float clearance = options.stepHeight + options.radius / -ground.normal.y + kSkin * scale;
        float lift = ground.point.y * scale - clearance - bottom;
        if (lift < 0.0F) {
            lift *= cast(position, {0.0F, lift}).fraction;
        }
        position.y += lift;
        groundNormal = ground.normal;
        groundShape = ground.shape;
        groundPoint = ground.point;
        const b2Vec2 anchor = b2Body_GetLocalPoint(b2Shape_GetBody(b2LoadShapeId(groundShape)), Query::meters(groundPoint));
        groundAnchor = {anchor.x, anchor.y};
    } else {
        groundNormal = {0.0F, -1.0F};
    }

    gatherPlanes(false);
    onWall = false;
    onCeiling = false;
    for (const Plane& plane : std::span(planes.data(), planeCount)) {
        onCeiling = onCeiling || plane.normal.y > 0.5F;
        onWall = onWall || (!isWalkable(plane.normal) && std::abs(plane.normal.x) >= 0.5F);
    }
}

// The point under the feet moves with the ground body, and the mover follows it.
math::Vec2 Mover::followGround() const {
    if (!grounded || !b2Shape_IsValid(b2LoadShapeId(groundShape))) {
        return {};
    }
    const b2Vec2 moved = b2Body_GetWorldPoint(b2Shape_GetBody(b2LoadShapeId(groundShape)), Query::meters(groundAnchor));
    return math::Vec2{moved.x - groundPoint.x, moved.y - groundPoint.y} * scale;
}

// Pushes the dynamic bodies the delta runs into toward the speed the mover asks for, with at most the push force.
void Mover::push(math::Vec2 delta) {
    const float seconds = world.getLastStep();
    if (options.pushForce <= 0.0F || seconds <= 0.0F) {
        return;
    }
    const b2Vec2 wanted = Box2DConverter::toMeters(delta, scale);
    const b2Vec2 middle = Box2DConverter::toMeters(position, scale);
    for (const Plane& plane : std::span(planes.data(), planeCount)) {
        const b2BodyId pushed = b2Shape_GetBody(b2LoadShapeId(plane.shape));
        const float into = -(wanted.x * plane.normal.x + wanted.y * plane.normal.y);
        if (into <= 0.0F || b2Body_GetType(pushed) != b2_dynamicBody) {
            continue;
        }
        // The mover pushes at the height of its middle, so a crate slides instead of tipping over its foot.
        const b2Vec2 direction = Query::meters(-plane.normal);
        const b2Vec2 point = b2Body_GetWorldCenterOfMass(pushed);
        const float missing = into / seconds - b2Dot(b2Body_GetLinearVelocity(pushed), direction);
        if (missing > 0.0F) {
            const float impulse = std::min(b2Body_GetMass(pushed) * missing, options.pushForce / scale * seconds);
            b2Body_ApplyLinearImpulse(pushed, b2MulSV(impulse, direction), {point.x, std::min(point.y, middle.y)}, true);
        }
    }
}

math::Vec2 Mover::move(math::Vec2 delta) {
    if (!body.isValid()) {
        throw std::logic_error("The mover was destroyed.");
    }
    const math::Vec2 start = position;
    const bool wasGrounded = grounded;
    const bool rising = delta.y < 0.0F;

    // On the ground the mover walks along it at the full sideways length, and the ground holds what pulls it down.
    math::Vec2 wanted = delta + followGround();
    if (wasGrounded && !rising) {
        wanted = math::Vec2{-groundNormal.y, groundNormal.x} * delta.x + followGround();
    }

    gatherPlanes(wasGrounded);
    push(wanted);
    slide(wanted, wasGrounded);
    if (rising) {
        grounded = false;
        groundNormal = {0.0F, -1.0F};
        gatherPlanes(false);
        onCeiling = std::ranges::any_of(std::span(planes.data(), planeCount), [](const Plane& plane) { return plane.normal.y > 0.5F; });
    } else {
        stand(wasGrounded);
    }
    moveBody();
    return position - start;
}

math::Vec2 Mover::clip(math::Vec2 velocity) const {
    for (const Plane& plane : std::span(planes.data(), planeCount)) {
        // Dynamic bodies give way to the pushes of the mover, so the speed toward them stays.
        const b2ShapeId shape = b2LoadShapeId(plane.shape);
        if (b2Shape_IsValid(shape) && b2Body_GetType(b2Shape_GetBody(shape)) == b2_dynamicBody) {
            continue;
        }
        velocity -= plane.normal * std::min(0.0F, math::Vec2::dot(velocity, plane.normal));
    }
    // Ground holds the mover straight up, so a landing on a slope stops it instead of turning the fall into a slide.
    if (grounded) {
        velocity.y = std::min(velocity.y, 0.0F);
    }
    return velocity;
}

void Mover::dropThrough(float seconds) {
    body.dropThrough(seconds);
    grounded = false;
}

void Mover::setPosition(math::Vec2 value) {
    position = value;
    body.setTransform(position, 0.0F);
    stand(false);
}

std::optional<Body> Mover::getGroundBody() const {
    if (!grounded || !b2Shape_IsValid(b2LoadShapeId(groundShape))) {
        return std::nullopt;
    }
    return Body(&world, b2StoreBodyId(b2Shape_GetBody(b2LoadShapeId(groundShape))));
}

math::Vec2 Mover::getGroundVelocity() const {
    if (!grounded || !b2Shape_IsValid(b2LoadShapeId(groundShape))) {
        return {};
    }
    return Box2DConverter::toPixels(b2Body_GetWorldPointVelocity(b2Shape_GetBody(b2LoadShapeId(groundShape)), Query::meters(groundPoint)), scale);
}

// The body follows by velocity over the next step, so what it touches is pushed instead of shoved apart.
void Mover::moveBody() {
    if (world.getLastStep() > 0.0F) {
        body.moveTo(position, 0.0F, world.getLastStep());
        return;
    }
    body.setTransform(position, 0.0F);
}

} // namespace haylen::physics2d
