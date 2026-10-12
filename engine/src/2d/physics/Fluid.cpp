#include "haylen/2d/physics/Fluid.hpp"

#include <box2d/box2d.h>

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>

#include "2d/physics/Box2DConverter.hpp"
#include "haylen/2d/physics/World.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/math/Math.hpp"

namespace haylen::physics2d {

// Collides the particles with the shapes of the world. It copies every shape near the particles in world units once per step, bins them in a grid, pushes each particle out of the shapes of its cell in parallel and sums the impulses the particles give each dynamic body.
class Fluid::Collider final {
  public:
    // A particle moves at most its top speed over the step and less than a smoothing radius by the density passes, so shapes reach that far beyond their bounds, which finds the shapes a particle crossed within the step.
    Collider(const Fluid& owner, std::span<const math::Vec2> points, float deltaSeconds) : fluid(owner), scale(owner.world.getPixelsPerMeter()), travel(owner.options.radius + owner.options.smoothingRadius + owner.options.maxSpeed * deltaSeconds), stepSeconds(deltaSeconds) {
        math::Vec2 low{std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
        math::Vec2 high = -low;
        for (const math::Vec2 point : points) {
            low = math::Vec2::min(low, point);
            high = math::Vec2::max(high, point);
        }
        const float margin = owner.options.radius * 2.0F;
        origin = low - math::Vec2{margin, margin};
        const math::Vec2 extent = high - low + math::Vec2{margin, margin} * 2.0F;
        cellSize = std::max({owner.options.smoothingRadius * 2.0F, extent.x / kMaxCells, extent.y / kMaxCells});
        columns = static_cast<int>(extent.x / cellSize) + 1;
        rows = static_cast<int>(extent.y / cellSize) + 1;
        gather(extent);
        bin();
        contacts.assign(points.size(), {});
    }

    // Pushes one particle out of the shapes around it and turns its velocity along them, moving its last position so the velocity the step reads from the positions is the one after the collision. A particle in a corner between two shapes can be pushed by one into the other, so the shapes push it again until none touches it, a few times at most.
    void collide(std::size_t index, math::Vec2& position, math::Vec2& last, float deltaSeconds) {
        const int column = std::clamp(static_cast<int>((position.x - origin.x) / cellSize), 0, columns - 1);
        const int row = std::clamp(static_cast<int>((position.y - origin.y) / cellSize), 0, rows - 1);
        const auto cell = static_cast<std::size_t>(row * columns + column);
        for (int pass = 0; pass < kPasses; ++pass) {
            bool touched = false;
            for (std::uint32_t entry = cellStarts[cell]; entry < cellStarts[cell + 1]; ++entry) {
                const Solid& solid = solids[cellEntries[entry]];
                math::Vec2 normal{};
                math::Vec2 surface{};
                if (solid.bounds.contains(position) && touch(solid, position, last, normal, surface)) {
                    const float depth = math::Vec2::dot(surface - position, normal) + fluid.options.radius;
                    resolve(index, solid, normal, surface, position, last, deltaSeconds);
                    touched = touched || depth > kSettled;
                }
            }
            if (!touched) {
                return;
            }
        }
    }

    // Gives every dynamic body the impulses its particles took from it. Each particle met the body as if the body could not move, so many particles together would throw a light body back faster than they hit it and feed energy into the liquid. The impulses of a body shrink until, weighted by the pushes, the body moves away from its particles no faster than they approached it and than gravity moved it into them over the step, so the body never bounces off the liquid and still floats. Only a push that would wake a body wakes it, so resting crates in still water fall asleep.
    void push() {
        for (const Contact& contact : contacts) {
            if (contact.valid) {
                Body& body = bodies[contact.body];
                body.impulse += contact.impulse;
                body.angularImpulse += math::Vec2::cross(contact.point - body.center, contact.impulse);
            }
        }
        for (const Contact& contact : contacts) {
            const float pushed = -math::Vec2::dot(contact.impulse, contact.normal);
            if (!contact.valid || pushed <= 0.0F) {
                continue;
            }
            Body& body = bodies[contact.body];
            const math::Vec2 arm = contact.point - body.center;
            const math::Vec2 change = body.impulse / body.mass + math::Vec2{-arm.y, arm.x} * (body.angularImpulse * body.inverseInertia);
            body.approach += pushed * (contact.approach + std::max(0.0F, math::Vec2::dot(body.fall, contact.normal)));
            body.recoil -= pushed * math::Vec2::dot(change, contact.normal);
        }
        for (Body& body : bodies) {
            if (body.recoil > body.approach) {
                body.share = body.approach / body.recoil;
            }
        }
        for (const Body& body : bodies) {
            if (!body.dynamic || (body.impulse.isZero() && body.angularImpulse == 0.0F)) {
                continue;
            }
            const b2BodyId id = b2LoadBodyId(body.id);
            const math::Vec2 impulse = body.impulse * body.share;
            const bool wakes = impulse.getLength() / body.mass > kWakeSpeed * scale;
            b2Body_ApplyLinearImpulseToCenter(id, Box2DConverter::toMeters(impulse, scale), wakes);
            b2Body_ApplyAngularImpulse(id, body.angularImpulse * body.share / (scale * scale), wakes);
        }
    }

  private:
    // Collisions of one particle repeat at most this many times, and only while a shape pushed it by more than this many world units.
    static constexpr int kPasses = 3;
    static constexpr float kSettled = 0.01F;

    enum class Kind : std::uint8_t {
        Circle,
        Capsule,
        Segment,
        OneSided,
        Polygon,
    };

    // A shape in world units, with the bounds a particle must be in to touch it.
    struct Solid {
        Kind kind = Kind::Circle;
        std::array<math::Vec2, B2_MAX_POLYGON_VERTICES> points{};
        std::array<math::Vec2, B2_MAX_POLYGON_VERTICES> normals{};
        int count = 0;
        float radius = 0.0F;
        math::Rect bounds{};
        std::uint32_t body = 0;
    };

    // A body in world units, with the impulses its particles gave it and the share of them it takes.
    struct Body {
        std::uint64_t id = 0;
        bool dynamic = false;
        float mass = 0.0F;
        float inverseInertia = 0.0F;
        math::Vec2 fall{};
        math::Vec2 center{};
        math::Vec2 velocity{};
        float angularVelocity = 0.0F;
        math::Vec2 impulse{};
        float angularImpulse = 0.0F;
        float approach = 0.0F;
        float recoil = 0.0F;
        float share = 1.0F;
    };

    // The push a particle gave a dynamic body in the step, where it touched it, and how fast it approached it there.
    struct Contact {
        std::uint32_t body = 0;
        math::Vec2 impulse{};
        math::Vec2 point{};
        math::Vec2 normal{};
        float approach = 0.0F;
        bool valid = false;
    };

    static constexpr float kMaxCells = 64.0F;
    // A particle push wakes a sleeping body only when it changes its speed by more than this many meters per second.
    static constexpr float kWakeSpeed = 0.05F;
    // Bodies without mass take the pushes of particles as if they had this many kilograms.
    static constexpr float kLeastMass = 1e-6F;

    // Moves a touching particle out to the surface, removes the part of its velocity that runs into the shape and slows its sliding by the friction, and records the push it gives a dynamic body.
    void resolve(std::size_t index, const Solid& solid, math::Vec2 normal, math::Vec2 surface, math::Vec2& position, math::Vec2& last, float deltaSeconds) {
        const Body& body = bodies[solid.body];
        const math::Vec2 before = (position - last) / deltaSeconds;
        const math::Vec2 arm = surface - body.center;
        const math::Vec2 carried = body.velocity + math::Vec2{-arm.y, arm.x} * body.angularVelocity;
        math::Vec2 relative = before - carried;
        const float approach = math::Vec2::dot(relative, normal);
        if (approach < 0.0F) {
            const math::Vec2 slide = relative - normal * approach;
            relative = slide * (1.0F - std::min(1.0F, fluid.options.friction)) - normal * (approach * fluid.options.restitution);
        }
        const math::Vec2 after = carried + relative;
        position = surface + normal * fluid.options.radius;
        last = position - after * deltaSeconds;
        if (body.dynamic) {
            Contact& contact = contacts[index];
            const bool same = contact.valid && contact.body == solid.body;
            contact.impulse = (same ? contact.impulse : math::Vec2{}) + (after - before) * -fluid.particleMass;
            contact.approach = std::max(same ? contact.approach : 0.0F, -approach);
            contact.body = solid.body;
            contact.point = surface;
            contact.normal = normal;
            contact.valid = true;
        }
    }

    [[nodiscard]] static math::Vec2 closestOnSegment(math::Vec2 point, math::Vec2 from, math::Vec2 to) noexcept {
        const math::Vec2 span = to - from;
        const float length = span.getLengthSquared();
        const float along = length > 0.0F ? std::clamp(math::Vec2::dot(point - from, span) / length, 0.0F, 1.0F) : 0.0F;
        return from + span * along;
    }

    // Returns where along the path from `from` to `to` a particle first comes within `reach` of a point, from 0 to 1, when the path starts out of reach and ends within it or passes it.
    [[nodiscard]] static std::optional<float> sweepPoint(math::Vec2 point, float reach, math::Vec2 from, math::Vec2 to) noexcept {
        const math::Vec2 path = to - from;
        const math::Vec2 offset = from - point;
        const float a = path.getLengthSquared();
        const float b = math::Vec2::dot(offset, path);
        const float c = offset.getLengthSquared() - reach * reach;
        const float discriminant = b * b - a * c;
        if (a <= 0.0F || c <= 0.0F || b >= 0.0F || discriminant < 0.0F) {
            return std::nullopt;
        }
        const float t = (-b - std::sqrt(discriminant)) / a;
        return t <= 1.0F ? std::optional<float>(t) : std::nullopt;
    }

    // Finds the first place where the path of a particle reached a round solid, a point or a segment grown by `reach`, which a fast particle passes within one step. Sets the normal out of the solid and the point of its surface there.
    [[nodiscard]] bool sweepRound(const Solid& solid, float reach, math::Vec2 last, math::Vec2 position, math::Vec2& normal, math::Vec2& surface) const {
        std::optional<float> first;
        math::Vec2 core{};
        const int ends = solid.kind == Kind::Circle ? 1 : 2;
        for (int end = 0; end < ends; ++end) {
            const math::Vec2 point = solid.points[static_cast<std::size_t>(end)];
            if (const std::optional<float> t = sweepPoint(point, reach, last, position); t && (!first || *t < *first)) {
                first = t;
                core = point;
            }
        }
        if (solid.kind != Kind::Circle) {
            // The path crosses the side of a segment where it comes within reach of its line between the ends.
            const math::Vec2 span = solid.points[1] - solid.points[0];
            const math::Vec2 side = math::Vec2{-span.y, span.x}.getNormalized();
            const float before = math::Vec2::dot(last - solid.points[0], side);
            const float after = math::Vec2::dot(position - solid.points[0], side);
            const float facing = before < 0.0F ? -1.0F : 1.0F;
            const float from = before * facing - reach;
            const float to = after * facing - reach;
            if (from > 0.0F && to <= 0.0F) {
                const float t = from / (from - to);
                const math::Vec2 crossing = math::Vec2::lerp(last, position, t);
                const float along = math::Vec2::dot(crossing - solid.points[0], span) / span.getLengthSquared();
                if (along >= 0.0F && along <= 1.0F && (!first || t < *first)) {
                    first = t;
                    core = solid.points[0] + span * along;
                }
            }
        }
        if (!first) {
            return false;
        }
        const math::Vec2 contact = math::Vec2::lerp(last, position, *first);
        normal = (contact - core).getNormalized();
        surface = core + normal * solid.radius;
        return true;
    }

    // Finds where the path of a particle first entered a polygon grown by `reach`, from the side it entered, which a fast particle passes within one step.
    [[nodiscard]] bool sweepPolygon(const Solid& solid, float reach, math::Vec2 last, math::Vec2 position, math::Vec2& normal, math::Vec2& surface) const {
        float enter = 0.0F;
        float exit = 1.0F;
        int face = -1;
        for (int index = 0; index < solid.count; ++index) {
            const auto slot = static_cast<std::size_t>(index);
            const float from = math::Vec2::dot(solid.normals[slot], last - solid.points[slot]) - reach;
            const float to = math::Vec2::dot(solid.normals[slot], position - solid.points[slot]) - reach;
            if (from > 0.0F && to > 0.0F) {
                return false;
            }
            if (from > 0.0F) {
                const float t = from / (from - to);
                if (t > enter || face < 0) {
                    enter = t;
                    face = index;
                }
            } else if (to > 0.0F) {
                exit = std::min(exit, from / (from - to));
            }
        }
        if (face < 0 || enter > exit) {
            return false;
        }
        normal = solid.normals[static_cast<std::size_t>(face)];
        surface = math::Vec2::lerp(last, position, enter) - normal * fluid.options.radius;
        return true;
    }

    // Finds where a particle touches a solid: the normal out of the solid and the point of its surface. A particle that comes from outside the reach of a solid stops where its path first reached it, which catches the particles that cross a thin solid within one step, and a particle that was touching a solid stays on the side it touched.
    [[nodiscard]] bool touch(const Solid& solid, math::Vec2 position, math::Vec2 last, math::Vec2& normal, math::Vec2& surface) const {
        const float reach = fluid.options.radius + solid.radius;
        switch (solid.kind) {
        case Kind::Circle:
        case Kind::Capsule:
        case Kind::Segment:
            return touchRound(solid, reach, position, last, normal, surface);
        case Kind::OneSided:
            return touchOneSided(solid, reach, position, last, normal, surface);
        case Kind::Polygon:
            return touchPolygon(solid, reach, position, last, normal, surface);
        }
        return false;
    }

    // Points, capsules and segments are a core point or segment grown by their radius.
    [[nodiscard]] bool touchRound(const Solid& solid, float reach, math::Vec2 position, math::Vec2 last, math::Vec2& normal, math::Vec2& surface) const {
        const bool point = solid.kind == Kind::Circle;
        const math::Vec2 lastCore = point ? solid.points[0] : closestOnSegment(last, solid.points[0], solid.points[1]);
        if ((last - lastCore).getLength() >= reach) {
            return sweepRound(solid, reach, last, position, normal, surface);
        }

        const math::Vec2 core = point ? solid.points[0] : closestOnSegment(position, solid.points[0], solid.points[1]);
        const math::Vec2 offset = position - core;
        const float distance = offset.getLength();
        const math::Vec2 held = (last - lastCore).getNormalized();
        if (math::Vec2::dot(offset, held) < 0.0F) {
            // The particle passed the core within the step, so it goes back to the side it touched.
            normal = held;
        } else if (distance >= reach) {
            return false;
        } else {
            normal = distance > 0.0F ? offset / distance : held;
        }
        surface = core + normal * solid.radius;
        return true;
    }

    // A one-sided segment stops the particles that come from its front side: those within reach of it along its length, those whose path crossed it within the step, and those near its ends like near a point.
    [[nodiscard]] bool touchOneSided(const Solid& solid, float reach, math::Vec2 position, math::Vec2 last, math::Vec2& normal, math::Vec2& surface) const {
        const math::Vec2 front = solid.normals[0];
        const float lastHeight = math::Vec2::dot(last - solid.points[0], front);
        if (lastHeight < 0.0F) {
            return false;
        }
        const math::Vec2 span = solid.points[1] - solid.points[0];
        const float height = math::Vec2::dot(position - solid.points[0], front);
        float along = math::Vec2::dot(position - solid.points[0], span) / span.getLengthSquared();
        if (height < 0.0F) {
            const math::Vec2 crossing = math::Vec2::lerp(last, position, lastHeight / (lastHeight - height));
            along = math::Vec2::dot(crossing - solid.points[0], span) / span.getLengthSquared();
        }
        if (along > 0.0F && along < 1.0F) {
            normal = front;
            surface = solid.points[0] + span * math::Vec2::dot(position - solid.points[0], span) / span.getLengthSquared();
            return height < reach;
        }
        const math::Vec2 core = solid.points[0] + span * std::clamp(along, 0.0F, 1.0F);
        const math::Vec2 offset = position - core;
        const float distance = offset.getLength();
        if (distance >= reach) {
            return false;
        }
        normal = distance > 0.0F ? offset / distance : front;
        surface = core;
        return true;
    }

    [[nodiscard]] bool touchPolygon(const Solid& solid, float reach, math::Vec2 position, math::Vec2 last, math::Vec2& normal, math::Vec2& surface) const {
        float deepest = -std::numeric_limits<float>::max();
        float held = -std::numeric_limits<float>::max();
        std::size_t side = 0;
        std::size_t face = 0;
        for (std::size_t slot = 0; slot < static_cast<std::size_t>(solid.count); ++slot) {
            const float separation = math::Vec2::dot(solid.normals[slot], position - solid.points[slot]);
            const float before = math::Vec2::dot(solid.normals[slot], last - solid.points[slot]);
            if (separation > deepest) {
                deepest = separation;
                side = slot;
            }
            if (before > held) {
                held = before;
                face = slot;
            }
        }
        if (held > reach) {
            return sweepPolygon(solid, reach, last, position, normal, surface);
        }

        // A particle that was inside the polygon leaves by its nearest side, and one that was touching it and is now behind the side it touched, inside the polygon or through it, goes back out of that side.
        const std::size_t exit = held > 0.0F ? face : side;
        const float behind = math::Vec2::dot(solid.normals[exit], position - solid.points[exit]);
        if (behind < 0.0F) {
            normal = solid.normals[exit];
            surface = position - normal * behind + normal * solid.radius;
            return true;
        }
        if (deepest > reach) {
            return false;
        }

        float nearest = std::numeric_limits<float>::max();
        math::Vec2 core{};
        for (std::size_t slot = 0; slot < static_cast<std::size_t>(solid.count); ++slot) {
            const math::Vec2 point = closestOnSegment(position, solid.points[slot], solid.points[(slot + 1) % static_cast<std::size_t>(solid.count)]);
            const float distance = (position - point).getLengthSquared();
            if (distance < nearest) {
                nearest = distance;
                core = point;
            }
        }
        const math::Vec2 offset = position - core;
        const float distance = offset.getLength();
        if (distance >= reach) {
            return false;
        }
        normal = distance > 0.0F ? offset / distance : solid.normals[exit];
        surface = core + normal * solid.radius;
        return true;
    }

    [[nodiscard]] std::uint32_t addBody(b2BodyId id) {
        const std::uint64_t key = b2StoreBodyId(id);
        for (std::size_t index = 0; index < bodies.size(); ++index) {
            if (bodies[index].id == key) {
                return static_cast<std::uint32_t>(index);
            }
        }
        const b2Vec2 velocity = b2Body_GetLinearVelocity(id);
        const float inertia = b2Body_GetRotationalInertia(id) * scale * scale;
        bodies.push_back({
            .id = key,
            .dynamic = b2Body_GetType(id) == b2_dynamicBody,
            .mass = std::max(b2Body_GetMass(id), kLeastMass),
            .inverseInertia = inertia > 0.0F ? 1.0F / inertia : 0.0F,
            .fall = fluid.world.getGravity() * (b2Body_GetGravityScale(id) * stepSeconds),
            .center = Box2DConverter::toPixels(b2Body_GetWorldCenterOfMass(id), scale),
            .velocity = {velocity.x * scale, velocity.y * scale},
            .angularVelocity = b2Body_GetAngularVelocity(id),
        });
        return static_cast<std::uint32_t>(bodies.size() - 1);
    }

    void add(b2ShapeId shape) {
        const b2BodyId owner = b2Shape_GetBody(shape);
        const b2Transform transform = b2Body_GetTransform(owner);
        const auto place = [&](b2Vec2 point) { return Box2DConverter::toPixels(b2TransformPoint(transform, point), scale); };
        Solid solid;
        switch (b2Shape_GetType(shape)) {
        case b2_circleShape: {
            const b2Circle circle = b2Shape_GetCircle(shape);
            solid.kind = Kind::Circle;
            solid.points[0] = place(circle.center);
            solid.radius = circle.radius * scale;
            break;
        }
        case b2_capsuleShape: {
            const b2Capsule capsule = b2Shape_GetCapsule(shape);
            solid.kind = Kind::Capsule;
            solid.points = {place(capsule.center1), place(capsule.center2)};
            solid.radius = capsule.radius * scale;
            break;
        }
        case b2_segmentShape: {
            const b2Segment segment = b2Shape_GetSegment(shape);
            solid.kind = Kind::Segment;
            solid.points = {place(segment.point1), place(segment.point2)};
            break;
        }
        case b2_chainSegmentShape: {
            const b2Segment segment = b2Shape_GetChainSegment(shape).segment;
            solid.kind = Kind::OneSided;
            solid.points = {place(segment.point1), place(segment.point2)};
            const math::Vec2 span = solid.points[1] - solid.points[0];
            solid.normals[0] = math::Vec2{span.y, -span.x}.getNormalized();
            break;
        }
        case b2_polygonShape: {
            const b2Polygon polygon = b2Shape_GetPolygon(shape);
            solid.kind = Kind::Polygon;
            solid.count = polygon.count;
            solid.radius = polygon.radius * scale;
            for (int index = 0; index < polygon.count; ++index) {
                const auto slot = static_cast<std::size_t>(index);
                solid.points[slot] = place(polygon.vertices[index]);
                const b2Vec2 normal = b2RotateVector(transform.q, polygon.normals[index]);
                solid.normals[slot] = {normal.x, normal.y};
            }
            break;
        }
        default:
            return;
        }

        const b2AABB box = b2Shape_GetAABB(shape);
        solid.bounds = math::Rect::fromMinMax(Box2DConverter::toPixels(box.lowerBound, scale) - math::Vec2{travel, travel}, Box2DConverter::toPixels(box.upperBound, scale) + math::Vec2{travel, travel});
        solid.body = addBody(owner);
        solids.push_back(solid);
    }

    void gather(math::Vec2 extent) {
        const b2AABB box{Box2DConverter::toMeters(origin, scale), Box2DConverter::toMeters(origin + extent, scale)};
        std::pair<const CollisionFilter*, std::vector<b2ShapeId>> found{&fluid.options.filter, {}};
        // clang-format off
        b2World_OverlapAABB(b2LoadWorldId(fluid.world.getHandle()), box, b2DefaultQueryFilter(), [](b2ShapeId shape, void* context) {
            auto& [filter, shapes] = *static_cast<std::pair<const CollisionFilter*, std::vector<b2ShapeId>>*>(context);
            if (!b2Shape_IsSensor(shape) && Box2DConverter::collides(*filter, b2Shape_GetFilter(shape))) {
                shapes.push_back(shape);
            }
            return true;
        }, &found);
        // clang-format on
        for (const b2ShapeId shape : found.second) {
            add(shape);
        }
    }

    // Lists every solid in each cell its bounds reach, cell by cell.
    void bin() {
        // clang-format off
        const auto cellRange = [this](const math::Rect& bounds) {
            const int left = std::clamp(static_cast<int>((bounds.getLeft() - origin.x) / cellSize), 0, columns - 1);
            const int right = std::clamp(static_cast<int>((bounds.getRight() - origin.x) / cellSize), 0, columns - 1);
            const int top = std::clamp(static_cast<int>((bounds.getTop() - origin.y) / cellSize), 0, rows - 1);
            const int bottom = std::clamp(static_cast<int>((bounds.getBottom() - origin.y) / cellSize), 0, rows - 1);
            return std::array<int, 4>{left, right, top, bottom};
        };
        // clang-format on
        cellStarts.assign(static_cast<std::size_t>(columns * rows) + 1, 0);
        for (const Solid& solid : solids) {
            const auto [left, right, top, bottom] = cellRange(solid.bounds);
            for (int row = top; row <= bottom; ++row) {
                for (int column = left; column <= right; ++column) {
                    ++cellStarts[static_cast<std::size_t>(row * columns + column) + 1];
                }
            }
        }
        for (std::size_t cell = 1; cell < cellStarts.size(); ++cell) {
            cellStarts[cell] += cellStarts[cell - 1];
        }
        cellEntries.resize(cellStarts.back());
        std::vector<std::uint32_t> cursors(cellStarts.begin(), cellStarts.end() - 1);
        for (std::size_t index = 0; index < solids.size(); ++index) {
            const auto [left, right, top, bottom] = cellRange(solids[index].bounds);
            for (int row = top; row <= bottom; ++row) {
                for (int column = left; column <= right; ++column) {
                    cellEntries[cursors[static_cast<std::size_t>(row * columns + column)]++] = static_cast<std::uint32_t>(index);
                }
            }
        }
    }

    const Fluid& fluid;
    float scale;
    float travel;
    float stepSeconds;
    math::Vec2 origin{};
    float cellSize = 1.0F;
    int columns = 1;
    int rows = 1;
    std::vector<Solid> solids;
    std::vector<Body> bodies;
    std::vector<std::uint32_t> cellStarts;
    std::vector<std::uint32_t> cellEntries;
    std::vector<Contact> contacts;
};

Fluid::Fluid(World& owner, const Options& settings) : world(owner), options(settings) {
    const bool materials = options.density >= 0.0F && options.friction >= 0.0F && options.restitution >= 0.0F && std::isfinite(options.gravityScale);
    const bool motion = options.viscosity >= 0.0F && options.viscosity <= 1.0F && options.iterations >= 1 && options.maxSpeed >= 0.0F;
    if (options.radius <= 0.0F || options.smoothingRadius <= options.radius || !materials || !motion) {
        throw std::invalid_argument("A fluid needs a positive particle radius, a larger smoothing radius, at least one iteration, a viscosity from 0 to 1, and a density, friction, restitution and top speed of zero or more.");
    }
    const float scale = world.getPixelsPerMeter();
    particleMass = options.density * math::Math::kPi * options.radius * options.radius / (scale * scale);

    // The liquid rests at the density of particles half a smoothing radius apart, which `fill` spawns.
    restDensity = 0.0F;
    for (int row = -2; row <= 2; ++row) {
        for (int column = -2; column <= 2; ++column) {
            restDensity += kernel(0.5F * std::hypot(static_cast<float>(column), static_cast<float>(row)));
        }
    }
    clumpKernel = kernel(kClumpDistance);
    hook = world.addStepHook(World::StepPhase::After, [this](float deltaSeconds) { step(deltaSeconds); });
}

Fluid::~Fluid() {
    world.removeStepHook(hook);
}

bool Fluid::spawn(math::Vec2 position, math::Vec2 velocity) {
    if (positions.size() >= options.maxParticles) {
        return false;
    }
    positions.push_back(position);
    previous.push_back(position);
    velocities.push_back(velocity);
    return true;
}

std::size_t Fluid::fill(const math::Rect& area, math::Vec2 velocity) {
    const float spacing = options.smoothingRadius * 0.5F;
    const auto columns = static_cast<int>(area.width / spacing);
    const auto rows = static_cast<int>(area.height / spacing);
    std::size_t added = 0;
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            // A perfect grid is a lattice the pressure balances, so every other row shifts by half a cell and every particle by a fixed small amount.
            const float shift = (row % 2 == 0 ? 0.25F : 0.75F) + 0.05F * static_cast<float>((column * 7 + row * 3) % 5 - 2);
            if (!spawn(area.getMin() + math::Vec2{static_cast<float>(column) + shift, static_cast<float>(row) + 0.5F} * spacing, velocity)) {
                return added;
            }
            ++added;
        }
    }
    return added;
}

void Fluid::remove(std::size_t index) {
    if (index >= positions.size()) {
        throw std::out_of_range("The fluid has no such particle.");
    }
    for (std::vector<math::Vec2>* values : {&positions, &previous, &velocities}) {
        (*values)[index] = values->back();
        values->pop_back();
    }
}

void Fluid::clear() {
    positions.clear();
    previous.clear();
    velocities.clear();
}

template <typename Work> void Fluid::forEachRange(Work&& work) {
    core::JobSystem* jobs = world.getJobs();
    if (jobs == nullptr) {
        work(std::size_t{0}, positions.size());
        return;
    }
    jobs->parallelFor(0, positions.size(), kGrain, work);
}

std::uint32_t Fluid::cellOf(int column, int row) const noexcept {
    const auto hash = static_cast<std::uint32_t>(column) * 73856093U ^ static_cast<std::uint32_t>(row) * 19349663U;
    return hash & cellMask;
}

// Sorts the particles by cell of the smoothing radius into a hash table with a counting sort.
void Fluid::buildGrid() {
    const std::size_t count = positions.size();
    const std::size_t table = std::bit_ceil(std::max<std::size_t>(count * 2, 64));
    cellMask = static_cast<std::uint32_t>(table - 1);
    const float inverse = 1.0F / options.smoothingRadius;

    cellKeys.resize(count);
    cellStarts.assign(table + 1, 0);
    for (std::size_t index = 0; index < count; ++index) {
        const std::uint32_t key = cellOf(static_cast<int>(std::floor(positions[index].x * inverse)), static_cast<int>(std::floor(positions[index].y * inverse)));
        cellKeys[index] = key;
        ++cellStarts[key + 1];
    }
    for (std::size_t cell = 1; cell <= table; ++cell) {
        cellStarts[cell] += cellStarts[cell - 1];
    }
    cellCursors.assign(cellStarts.begin(), cellStarts.end() - 1);
    sorted.resize(count);
    for (std::size_t index = 0; index < count; ++index) {
        sorted[cellCursors[cellKeys[index]]++] = static_cast<std::uint32_t>(index);
    }
}

float Fluid::kernel(float distance) noexcept {
    const float weight = std::max(0.0F, 1.0F - distance);
    return weight * weight;
}

float Fluid::slope(float distance) noexcept {
    return -2.0F * std::max(0.0F, 1.0F - distance);
}

// Lists the neighbors of each particle within the smoothing radius.
void Fluid::findNeighbors(std::size_t begin, std::size_t end) {
    const float size = options.smoothingRadius;
    const float inverse = 1.0F / size;
    for (std::size_t index = begin; index < end; ++index) {
        const math::Vec2 position = positions[index];
        const int column = static_cast<int>(std::floor(position.x * inverse));
        const int row = static_cast<int>(std::floor(position.y * inverse));

        // Neighbor cells that share a bucket of the table are read once.
        std::array<std::uint32_t, 9> buckets{};
        std::size_t bucketCount = 0;
        for (int offsetY = -1; offsetY <= 1; ++offsetY) {
            for (int offsetX = -1; offsetX <= 1; ++offsetX) {
                const std::uint32_t bucket = cellOf(column + offsetX, row + offsetY);
                if (std::find(buckets.begin(), buckets.begin() + static_cast<std::ptrdiff_t>(bucketCount), bucket) == buckets.begin() + static_cast<std::ptrdiff_t>(bucketCount)) {
                    buckets[bucketCount++] = bucket;
                }
            }
        }

        std::uint32_t* list = &neighbors[index * kMaxNeighbors];
        std::size_t found = 0;
        for (std::size_t bucket = 0; bucket < bucketCount && found < kMaxNeighbors; ++bucket) {
            for (std::uint32_t entry = cellStarts[buckets[bucket]]; entry < cellStarts[buckets[bucket] + 1] && found < kMaxNeighbors; ++entry) {
                const std::uint32_t other = sorted[entry];
                if (other != index && (positions[other] - position).getLengthSquared() < size * size) {
                    list[found++] = other;
                }
            }
        }
        neighborCounts[index] = static_cast<std::uint8_t>(found);
    }
}

// Finds how strongly each compressed particle must push its neighbors away to bring its density back to rest: the density error over the squared slopes of the error, which keeps the push in proportion to the compression whatever the number of neighbors. Particles below the rest density do not pull, so the surface stays free. A compressed particle always has a neighbor within the smoothing radius, so the slopes never sum to zero.
void Fluid::solveDensity(std::size_t begin, std::size_t end) {
    const float inverse = 1.0F / options.smoothingRadius;
    for (std::size_t index = begin; index < end; ++index) {
        float density = kernel(0.0F);
        math::Vec2 own{};
        float others = 0.0F;
        const std::uint32_t* list = &neighbors[index * kMaxNeighbors];
        for (std::size_t item = 0; item < neighborCounts[index]; ++item) {
            const math::Vec2 offset = (positions[index] - positions[list[item]]) * inverse;
            const float distance = offset.getLength();
            if (distance >= 1.0F || distance <= 0.0F) {
                continue;
            }
            density += kernel(distance);
            const math::Vec2 gradient = offset * (slope(distance) / (distance * restDensity));
            own += gradient;
            others += gradient.getLengthSquared();
        }
        const float error = density / restDensity - 1.0F;
        lambdas[index] = error > 0.0F ? -error / (own.getLengthSquared() + others) : 0.0F;
    }
}

// Moves each particle away from its neighbors by the pushes they share, and a little more from neighbors that come close enough to clump, at most by `limit` world units. Each particle only writes its own new position, so the pass splits over the workers.
void Fluid::separate(std::size_t begin, std::size_t end, float limit) {
    const float inverse = 1.0F / options.smoothingRadius;
    for (std::size_t index = begin; index < end; ++index) {
        math::Vec2 shift{};
        const std::uint32_t* list = &neighbors[index * kMaxNeighbors];
        for (std::size_t item = 0; item < neighborCounts[index]; ++item) {
            const std::uint32_t other = list[item];
            const math::Vec2 offset = (positions[index] - positions[other]) * inverse;
            const float distance = offset.getLength();
            if (distance >= 1.0F || distance <= 0.0F) {
                continue;
            }
            const float ratio = kernel(distance) / clumpKernel;
            const float clump = -kClumpStrength * ratio * ratio * ratio * ratio;
            shift += offset * ((lambdas[index] + lambdas[other] + clump) * slope(distance) / (distance * restDensity));
        }
        scratch[index] = positions[index] + (shift * (options.smoothingRadius * kRelaxation)).clampedLength(limit);
    }
}

// Blends the velocity of each particle toward those of its neighbors by the viscosity, which keeps the momentum of the liquid and calms the motion within it, and caps the speed.
void Fluid::smooth(std::size_t begin, std::size_t end) {
    const float inverse = 1.0F / options.smoothingRadius;
    for (std::size_t index = begin; index < end; ++index) {
        math::Vec2 change{};
        float total = 0.0F;
        const std::uint32_t* list = &neighbors[index * kMaxNeighbors];
        for (std::size_t item = 0; item < neighborCounts[index]; ++item) {
            const std::uint32_t other = list[item];
            const float weight = kernel((positions[index] - positions[other]).getLength() * inverse);
            change += (velocities[other] - velocities[index]) * weight;
            total += weight;
        }
        const math::Vec2 blended = total > 0.0F ? velocities[index] + change * (options.viscosity / std::max(total, 1.0F)) : velocities[index];
        scratch[index] = blended.clampedLength(options.maxSpeed);
    }
}

void Fluid::step(float deltaSeconds) {
    const std::size_t count = positions.size();
    if (count == 0 || deltaSeconds <= 0.0F) {
        stepMilliseconds = 0.0F;
        return;
    }
    const auto start = std::chrono::steady_clock::now();
    const math::Vec2 gravity = world.getGravity() * (options.gravityScale * deltaSeconds);

    // Gravity moves the particles first, and the density passes and the collisions then correct the positions they reach.
    // clang-format off
    forEachRange([&](std::size_t begin, std::size_t end) {
        for (std::size_t index = begin; index < end; ++index) {
            velocities[index] += gravity;
            previous[index] = positions[index];
            positions[index] += velocities[index] * deltaSeconds;
        }
    });
    // clang-format on

    buildGrid();
    neighbors.resize(count * kMaxNeighbors);
    neighborCounts.resize(count);
    lambdas.resize(count);
    scratch.resize(count);
    forEachRange([this](std::size_t begin, std::size_t end) { findNeighbors(begin, end); });

    // The density passes push the compressed particles apart, and the collisions then move them out of the shapes once, so the velocity a particle leaves a shape with is the one it keeps.
    const float limit = kPushSpeed * world.getPixelsPerMeter() * deltaSeconds / static_cast<float>(options.iterations);
    for (int iteration = 0; iteration < options.iterations; ++iteration) {
        forEachRange([this](std::size_t begin, std::size_t end) { solveDensity(begin, end); });
        forEachRange([this, limit](std::size_t begin, std::size_t end) { separate(begin, end, limit); });
        positions.swap(scratch);
    }
    Collider collider(*this, positions, deltaSeconds);
    // clang-format off
    forEachRange([&](std::size_t begin, std::size_t end) {
        for (std::size_t index = begin; index < end; ++index) {
            collider.collide(index, positions[index], previous[index], deltaSeconds);
        }
    });
    // clang-format on
    collider.push();

    // clang-format off
    forEachRange([&](std::size_t begin, std::size_t end) {
        for (std::size_t index = begin; index < end; ++index) {
            velocities[index] = (positions[index] - previous[index]) / deltaSeconds;
        }
    });
    // clang-format on
    forEachRange([this](std::size_t begin, std::size_t end) { smooth(begin, end); });
    velocities.swap(scratch);
    stepMilliseconds = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - start).count();
}

} // namespace haylen::physics2d
