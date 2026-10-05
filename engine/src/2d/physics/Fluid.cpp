#include "haylen/2d/physics/Fluid.hpp"

#include <box2d/box2d.h>

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <limits>
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
    // A particle moves at most its top speed over the step, so shapes reach that far beyond their bounds, which catches particles that crossed a thin shape within the step.
    Collider(const Fluid& owner, std::span<const math::Vec2> points, float deltaSeconds) : fluid(owner), scale(owner.world.getPixelsPerMeter()), travel(owner.options.radius + owner.options.maxSpeed * deltaSeconds) {
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

    // Gives every dynamic body the impulses its particles took from it. Only a push that would wake a body wakes it, so resting crates in still water fall asleep.
    void push() {
        for (const Contact& contact : contacts) {
            if (contact.valid) {
                Body& body = bodies[contact.body];
                body.impulse += contact.impulse;
                body.angularImpulse += math::Vec2::cross(contact.point - body.center, contact.impulse);
            }
        }
        for (const Body& body : bodies) {
            if (!body.dynamic || (body.impulse.isZero() && body.angularImpulse == 0.0F)) {
                continue;
            }
            const b2BodyId id = b2LoadBodyId(body.id);
            const bool wakes = body.impulse.getLength() / std::max(body.mass, 1e-6F) > kWakeSpeed * scale;
            b2Body_ApplyLinearImpulseToCenter(id, Box2DConverter::toMeters(body.impulse, scale), wakes);
            b2Body_ApplyAngularImpulse(id, body.angularImpulse / (scale * scale), wakes);
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

    struct Body {
        std::uint64_t id = 0;
        bool dynamic = false;
        float mass = 0.0F;
        math::Vec2 center{};
        math::Vec2 velocity{};
        float angularVelocity = 0.0F;
        math::Vec2 impulse{};
        float angularImpulse = 0.0F;
    };

    struct Contact {
        std::uint32_t body = 0;
        math::Vec2 impulse{};
        math::Vec2 point{};
        bool valid = false;
    };

    static constexpr float kMaxCells = 64.0F;
    // A particle push wakes a sleeping body only when it changes its speed by more than this many meters per second.
    static constexpr float kWakeSpeed = 0.05F;

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
            contact.impulse = contact.valid && contact.body == solid.body ? contact.impulse + (after - before) * -fluid.particleMass : (after - before) * -fluid.particleMass;
            contact.body = solid.body;
            contact.point = surface;
            contact.valid = true;
        }
    }

    [[nodiscard]] static math::Vec2 closestOnSegment(math::Vec2 point, math::Vec2 from, math::Vec2 to) noexcept {
        const math::Vec2 span = to - from;
        const float length = span.getLengthSquared();
        const float along = length > 0.0F ? std::clamp(math::Vec2::dot(point - from, span) / length, 0.0F, 1.0F) : 0.0F;
        return from + span * along;
    }

    // Finds where a particle touches a solid: the normal out of the solid and the point of its surface, which a particle inside a polygon leaves by the nearest side.
    [[nodiscard]] bool touch(const Solid& solid, math::Vec2 position, math::Vec2 last, math::Vec2& normal, math::Vec2& surface) const {
        const float reach = fluid.options.radius + solid.radius;
        math::Vec2 core{};
        switch (solid.kind) {
        case Kind::Circle:
            core = solid.points[0];
            break;
        case Kind::Capsule:
        case Kind::Segment:
            core = closestOnSegment(position, solid.points[0], solid.points[1]);
            break;
        case Kind::OneSided: {
            // A one-sided segment stops the particles that come from its front side: those within reach of it along its length, those whose path crossed it within the step, and those near its ends like near a point.
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
            core = solid.points[0] + span * std::clamp(along, 0.0F, 1.0F);
            if (along > 0.0F && along < 1.0F) {
                normal = front;
                surface = solid.points[0] + span * math::Vec2::dot(position - solid.points[0], span) / span.getLengthSquared();
                return height < reach;
            }
            break;
        }
        case Kind::Polygon: {
            float deepest = -std::numeric_limits<float>::max();
            float entered = -std::numeric_limits<float>::max();
            int side = 0;
            int entry = 0;
            for (int index = 0; index < solid.count; ++index) {
                const auto slot = static_cast<std::size_t>(index);
                const float separation = math::Vec2::dot(solid.normals[slot], position - solid.points[slot]);
                const float before = math::Vec2::dot(solid.normals[slot], last - solid.points[slot]);
                if (separation > deepest) {
                    deepest = separation;
                    side = index;
                }
                if (before > entered) {
                    entered = before;
                    entry = index;
                }
            }
            if (deepest > reach) {
                return false;
            }
            if (deepest < 0.0F) {
                // A particle inside leaves through the side it came in, which a particle pushed deep in one step would otherwise cross.
                const auto face = static_cast<std::size_t>(entered > 0.0F ? entry : side);
                normal = solid.normals[face];
                surface = position - normal * math::Vec2::dot(normal, position - solid.points[face]) + normal * solid.radius;
                return true;
            }
            float nearest = std::numeric_limits<float>::max();
            for (int index = 0; index < solid.count; ++index) {
                const math::Vec2 point = closestOnSegment(position, solid.points[static_cast<std::size_t>(index)], solid.points[static_cast<std::size_t>((index + 1) % solid.count)]);
                const float distance = (position - point).getLengthSquared();
                if (distance < nearest) {
                    nearest = distance;
                    core = point;
                }
            }
            break;
        }
        }

        const math::Vec2 offset = position - core;
        const float distance = offset.getLength();
        if (distance >= reach) {
            return false;
        }
        normal = distance > 0.0F ? offset / distance : math::Vec2{0.0F, -1.0F};
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
        bodies.push_back({
            .id = key,
            .dynamic = b2Body_GetType(id) == b2_dynamicBody,
            .mass = b2Body_GetMass(id),
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
    const bool pressures = options.restDensity >= 0.0F && options.stiffness >= 0.0F && options.nearStiffness >= 0.0F && options.viscosity >= 0.0F && options.maxSpeed >= 0.0F;
    if (options.radius <= 0.0F || options.smoothingRadius <= options.radius || !materials || !pressures) {
        throw std::invalid_argument("A fluid needs a positive particle radius, a larger smoothing radius, and a density, friction, restitution, rest density, stiffness, viscosity and top speed of zero or more.");
    }
    const float scale = world.getPixelsPerMeter();
    particleMass = options.density * math::Math::kPi * options.radius * options.radius / (scale * scale);
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

// Lists the neighbors of each particle within the smoothing radius and sums its density and near density.
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

        Neighbor* list = &neighbors[index * kMaxNeighbors];
        std::size_t found = 0;
        float density = 0.0F;
        float nearDensity = 0.0F;
        for (std::size_t bucket = 0; bucket < bucketCount && found < kMaxNeighbors; ++bucket) {
            for (std::uint32_t entry = cellStarts[buckets[bucket]]; entry < cellStarts[buckets[bucket] + 1] && found < kMaxNeighbors; ++entry) {
                const std::uint32_t other = sorted[entry];
                const math::Vec2 offset = positions[other] - position;
                const float squared = offset.getLengthSquared();
                if (other == index || squared >= size * size || squared <= 0.0F) {
                    continue;
                }
                const float distance = std::sqrt(squared);
                const float weight = 1.0F - distance * inverse;
                list[found++] = {.index = other, .weight = weight, .direction = offset / distance};
                density += weight * weight;
                nearDensity += weight * weight * weight;
            }
        }
        neighborCounts[index] = static_cast<std::uint8_t>(found);
        densities[index] = density;
        nearDensities[index] = nearDensity;
    }
}

// Moves each particle by the pressures it shares with its neighbors. Each particle only writes its own new position, so the pass splits over the workers.
void Fluid::relax(std::size_t begin, std::size_t end, float frames) {
    const float scale = frames * frames * options.smoothingRadius * 0.5F;
    for (std::size_t index = begin; index < end; ++index) {
        const float pressure = options.stiffness * (densities[index] - options.restDensity);
        const float nearPressure = options.nearStiffness * nearDensities[index];
        math::Vec2 shift{};
        const Neighbor* list = &neighbors[index * kMaxNeighbors];
        for (std::size_t item = 0; item < neighborCounts[index]; ++item) {
            const Neighbor& neighbor = list[item];
            const float shared = (pressure + options.stiffness * (densities[neighbor.index] - options.restDensity)) * 0.5F;
            const float sharedNear = (nearPressure + options.nearStiffness * nearDensities[neighbor.index]) * 0.5F;
            shift -= neighbor.direction * ((shared * neighbor.weight + sharedNear * neighbor.weight * neighbor.weight) * scale);
        }
        scratch[index] = positions[index] + shift;
    }
}

// Slows particles that move toward each other, which evens out the speeds of neighbors.
void Fluid::smooth(std::size_t begin, std::size_t end, float frames) {
    for (std::size_t index = begin; index < end; ++index) {
        math::Vec2 change{};
        const Neighbor* list = &neighbors[index * kMaxNeighbors];
        for (std::size_t item = 0; item < neighborCounts[index]; ++item) {
            const Neighbor& neighbor = list[item];
            const float approach = math::Vec2::dot(velocities[index] - velocities[neighbor.index], neighbor.direction);
            if (approach > 0.0F) {
                change -= neighbor.direction * (std::min(1.0F, frames * neighbor.weight * options.viscosity) * approach * 0.5F);
            }
        }
        scratch[index] = (velocities[index] + change).clampedLength(options.maxSpeed);
    }
}

void Fluid::step(float deltaSeconds) {
    const std::size_t count = positions.size();
    if (count == 0 || deltaSeconds <= 0.0F) {
        stepMilliseconds = 0.0F;
        return;
    }
    const auto start = std::chrono::steady_clock::now();
    const float frames = deltaSeconds * 60.0F;
    const math::Vec2 gravity = world.getGravity() * (options.gravityScale * deltaSeconds);

    // Gravity moves the particles first, and the relaxation then corrects the positions they reach.
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
    densities.resize(count);
    nearDensities.resize(count);
    scratch.resize(count);
    forEachRange([this](std::size_t begin, std::size_t end) { findNeighbors(begin, end); });
    forEachRange([this, frames](std::size_t begin, std::size_t end) { relax(begin, end, frames); });
    positions.swap(scratch);

    Collider collider(*this, positions, deltaSeconds);
    // clang-format off
    forEachRange([&](std::size_t begin, std::size_t end) {
        for (std::size_t index = begin; index < end; ++index) {
            collider.collide(index, positions[index], previous[index], deltaSeconds);
            velocities[index] = (positions[index] - previous[index]) / deltaSeconds;
        }
    });
    // clang-format on
    collider.push();

    forEachRange([this, frames](std::size_t begin, std::size_t end) { smooth(begin, end, frames); });
    velocities.swap(scratch);
    stepMilliseconds = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - start).count();
}

} // namespace haylen::physics2d
