#include "haylen/2d/physics/World.hpp"

#include <box2d/box2d.h>

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "2d/physics/Box2DConverter.hpp"
#include "2d/physics/DebugDraw.hpp"
#include "haylen/2d/physics/Raycaster.hpp"

namespace haylen::physics2d {

const World::Settings World::kDefaultSettings{};
debug::ObjectCounter World::bodyCounter("PhysicsBody", debug::ObjectCounter::Kind::Native);
debug::ObjectCounter World::contactCounter("PhysicsContact", debug::ObjectCounter::Kind::Native);

// Decides in the pre-solve callback of Box2D whether a contact with a one-way platform holds, which Box2D may call from several threads at once. It only reads the transforms of bodies, which stay still while Box2D collides.
class World::OneWayFilter final {
  public:
    static bool preSolve(b2ShapeId shapeA, b2ShapeId shapeB, b2Manifold* manifold, void* context) {
        const auto& platforms = static_cast<const World*>(context)->oneWayShapes;
        const math::Vec2 normal{manifold->normal.x, manifold->normal.y};

        // The manifold normal points from the first shape to the second.
        return holds(platforms, shapeA, normal) && holds(platforms, shapeB, -normal);
    }

  private:
    // Returns whether a contact whose normal points away from the shape holds, turning the direction of a one-way platform with its body.
    static bool holds(const std::unordered_map<std::uint64_t, math::Vec2>& platforms, b2ShapeId shape, math::Vec2 normal) {
        const auto found = platforms.find(b2StoreShapeId(shape));
        if (found == platforms.end()) {
            return true;
        }
        const b2Vec2 direction = b2RotateVector(b2Body_GetRotation(b2Shape_GetBody(shape)), {found->second.x, found->second.y});
        return math::Vec2::dot(normal, {direction.x, direction.y}) >= kOneWayThreshold;
    }
};

World::World(const Settings& settings) : pixelsPerMeter(settings.pixelsPerMeter), subSteps(settings.subSteps) {
    if (settings.pixelsPerMeter <= 0.0F || settings.subSteps < 1) {
        throw std::invalid_argument("A physics world needs positive pixels per meter and at least one sub-step.");
    }
    b2WorldDef def = b2DefaultWorldDef();
    def.gravity = Box2DConverter::toMeters(settings.gravity, pixelsPerMeter);
    handle = b2StoreWorldId(b2CreateWorld(&def));
    b2World_SetPreSolveCallback(b2LoadWorldId(handle), &OneWayFilter::preSolve, this);
}

World::~World() {
    b2DestroyWorld(b2LoadWorldId(handle));
}

Body World::createBody(const Body::Options& options) {
    b2BodyDef def = b2DefaultBodyDef();
    def.type = Box2DConverter::toBodyType(options.type);
    def.position = Box2DConverter::toMeters(options.position, pixelsPerMeter);
    def.rotation = b2MakeRot(options.rotation);
    def.linearVelocity = Box2DConverter::toMeters(options.velocity, pixelsPerMeter);
    def.angularVelocity = options.angularVelocity;
    def.linearDamping = options.linearDamping;
    def.angularDamping = options.angularDamping;
    def.gravityScale = options.gravityScale;
    def.fixedRotation = options.fixedRotation;
    def.isBullet = options.bullet;
    def.enableSleep = options.sleepEnabled;
    const Body body{this, b2StoreBodyId(b2CreateBody(b2LoadWorldId(handle), &def))};
    countObjects();
    return body;
}

void World::countObjects() {
    const b2Counters counters = b2World_GetCounters(b2LoadWorldId(handle));
    liveBodies.set(static_cast<std::size_t>(counters.bodyCount));
    liveContacts.set(static_cast<std::size_t>(counters.contactCount));
}

Joint World::createJoint(Joint::Type type, Body first, Body second, const Joint::Options& options) {
    if (!first.isValid() || !second.isValid() || first.getWorld() != this || second.getWorld() != this) {
        throw std::invalid_argument("A joint needs two bodies of this world.");
    }

    const b2WorldId world = b2LoadWorldId(handle);
    const b2BodyId a = b2LoadBodyId(first.getId());
    const b2BodyId b = b2LoadBodyId(second.getId());
    const b2Vec2 anchorA = Box2DConverter::toMeters(options.anchorA, pixelsPerMeter);
    const b2Vec2 anchorB = Box2DConverter::toMeters(options.anchorB, pixelsPerMeter);
    const float maxForce = options.maxMotorForce / pixelsPerMeter;
    const float maxTorque = options.maxMotorTorque / (pixelsPerMeter * pixelsPerMeter);
    b2JointId joint{};

    switch (type) {
    case Joint::Type::Distance: {
        b2DistanceJointDef def = b2DefaultDistanceJointDef();
        def.bodyIdA = a;
        def.bodyIdB = b;
        def.localAnchorA = b2Body_GetLocalPoint(a, anchorA);
        def.localAnchorB = b2Body_GetLocalPoint(b, anchorB);
        def.length = options.length > 0.0F ? options.length / pixelsPerMeter : b2Distance(anchorA, anchorB);
        def.enableSpring = options.enableSpring;
        def.hertz = options.hertz;
        def.dampingRatio = options.dampingRatio;
        def.enableLimit = options.enableLimit;
        def.minLength = options.lower / pixelsPerMeter;
        def.maxLength = options.upper / pixelsPerMeter;
        def.collideConnected = options.collideConnected;
        joint = b2CreateDistanceJoint(world, &def);
        break;
    }
    case Joint::Type::Revolute: {
        b2RevoluteJointDef def = b2DefaultRevoluteJointDef();
        def.bodyIdA = a;
        def.bodyIdB = b;
        def.localAnchorA = b2Body_GetLocalPoint(a, anchorA);
        def.localAnchorB = b2Body_GetLocalPoint(b, anchorA);
        def.enableLimit = options.enableLimit;
        def.lowerAngle = options.lower;
        def.upperAngle = options.upper;
        def.enableMotor = options.enableMotor;
        def.motorSpeed = options.motorSpeed;
        def.maxMotorTorque = maxTorque;
        def.enableSpring = options.enableSpring;
        def.hertz = options.hertz;
        def.dampingRatio = options.dampingRatio;
        def.collideConnected = options.collideConnected;
        joint = b2CreateRevoluteJoint(world, &def);
        break;
    }
    case Joint::Type::Prismatic: {
        b2PrismaticJointDef def = b2DefaultPrismaticJointDef();
        def.bodyIdA = a;
        def.bodyIdB = b;
        def.localAnchorA = b2Body_GetLocalPoint(a, anchorA);
        def.localAnchorB = b2Body_GetLocalPoint(b, anchorA);
        def.localAxisA = b2Body_GetLocalVector(a, b2Normalize(b2Vec2{options.axis.x, options.axis.y}));
        def.enableLimit = options.enableLimit;
        def.lowerTranslation = options.lower / pixelsPerMeter;
        def.upperTranslation = options.upper / pixelsPerMeter;
        def.enableMotor = options.enableMotor;
        def.motorSpeed = options.motorSpeed / pixelsPerMeter;
        def.maxMotorForce = maxForce;
        def.enableSpring = options.enableSpring;
        def.hertz = options.hertz;
        def.dampingRatio = options.dampingRatio;
        def.collideConnected = options.collideConnected;
        joint = b2CreatePrismaticJoint(world, &def);
        break;
    }
    case Joint::Type::Weld: {
        b2WeldJointDef def = b2DefaultWeldJointDef();
        def.bodyIdA = a;
        def.bodyIdB = b;
        def.localAnchorA = b2Body_GetLocalPoint(a, anchorA);
        def.localAnchorB = b2Body_GetLocalPoint(b, anchorA);
        def.referenceAngle = b2RelativeAngle(b2Body_GetRotation(b), b2Body_GetRotation(a));
        def.linearHertz = options.hertz;
        def.angularHertz = options.hertz;
        def.linearDampingRatio = options.dampingRatio;
        def.angularDampingRatio = options.dampingRatio;
        def.collideConnected = options.collideConnected;
        joint = b2CreateWeldJoint(world, &def);
        break;
    }
    case Joint::Type::Wheel: {
        b2WheelJointDef def = b2DefaultWheelJointDef();
        def.bodyIdA = a;
        def.bodyIdB = b;
        def.localAnchorA = b2Body_GetLocalPoint(a, anchorA);
        def.localAnchorB = b2Body_GetLocalPoint(b, anchorA);
        def.localAxisA = b2Body_GetLocalVector(a, b2Normalize(b2Vec2{options.axis.x, options.axis.y}));
        def.enableSpring = options.enableSpring;
        def.hertz = options.hertz;
        def.dampingRatio = options.dampingRatio;
        def.enableLimit = options.enableLimit;
        def.lowerTranslation = options.lower / pixelsPerMeter;
        def.upperTranslation = options.upper / pixelsPerMeter;
        def.enableMotor = options.enableMotor;
        def.motorSpeed = options.motorSpeed;
        def.maxMotorTorque = maxTorque;
        def.collideConnected = options.collideConnected;
        joint = b2CreateWheelJoint(world, &def);
        break;
    }
    case Joint::Type::Mouse: {
        b2MouseJointDef def = b2DefaultMouseJointDef();
        def.bodyIdA = a;
        def.bodyIdB = b;
        def.target = anchorB;
        def.hertz = options.hertz > 0.0F ? options.hertz : def.hertz;
        def.dampingRatio = options.dampingRatio > 0.0F ? options.dampingRatio : def.dampingRatio;
        def.maxForce = maxForce > 0.0F ? maxForce : 1000.0F * b2Body_GetMass(b);
        def.collideConnected = options.collideConnected;
        joint = b2CreateMouseJoint(world, &def);
        break;
    }
    case Joint::Type::Motor: {
        b2MotorJointDef def = b2DefaultMotorJointDef();
        def.bodyIdA = a;
        def.bodyIdB = b;
        def.linearOffset = b2Body_GetLocalPoint(a, b2Body_GetPosition(b));
        def.angularOffset = b2RelativeAngle(b2Body_GetRotation(b), b2Body_GetRotation(a));
        def.maxForce = maxForce;
        def.maxTorque = maxTorque;
        def.collideConnected = options.collideConnected;
        joint = b2CreateMotorJoint(world, &def);
        break;
    }
    case Joint::Type::Filter: {
        b2FilterJointDef def = b2DefaultFilterJointDef();
        def.bodyIdA = a;
        def.bodyIdB = b;
        joint = b2CreateFilterJoint(world, &def);
        break;
    }
    }
    return {this, b2StoreJointId(joint)};
}

void World::step(float deltaSeconds) {
    const b2WorldId world = b2LoadWorldId(handle);
    std::erase_if(oneWayShapes, [](const auto& platform) { return !b2Shape_IsValid(b2LoadShapeId(platform.first)); });
    b2World_Step(world, deltaSeconds, subSteps);

    contactBegins.clear();
    contactEnds.clear();
    contactHits.clear();
    sensorBegins.clear();
    sensorEnds.clear();

    const b2ContactEvents contacts = b2World_GetContactEvents(world);
    for (int index = 0; index < contacts.beginCount; ++index) {
        const b2ContactBeginTouchEvent& event = contacts.beginEvents[index];
        ContactEvent begin{.first = {this, b2StoreShapeId(event.shapeIdA)}, .second = {this, b2StoreShapeId(event.shapeIdB)}, .normal = {event.manifold.normal.x, event.manifold.normal.y}};
        if (event.manifold.pointCount > 0) {
            begin.point = Box2DConverter::toPixels(event.manifold.points[0].point, pixelsPerMeter);
        }
        contactBegins.push_back(begin);
    }
    for (int index = 0; index < contacts.endCount; ++index) {
        const b2ContactEndTouchEvent& event = contacts.endEvents[index];
        contactEnds.push_back({.first = {this, b2StoreShapeId(event.shapeIdA)}, .second = {this, b2StoreShapeId(event.shapeIdB)}});
    }
    for (int index = 0; index < contacts.hitCount; ++index) {
        const b2ContactHitEvent& event = contacts.hitEvents[index];
        contactHits.push_back({.first = {this, b2StoreShapeId(event.shapeIdA)}, .second = {this, b2StoreShapeId(event.shapeIdB)}, .point = Box2DConverter::toPixels(event.point, pixelsPerMeter), .normal = {event.normal.x, event.normal.y}, .speed = event.approachSpeed * pixelsPerMeter});
    }

    const b2SensorEvents sensors = b2World_GetSensorEvents(world);
    for (int index = 0; index < sensors.beginCount; ++index) {
        sensorBegins.push_back({.sensor = {this, b2StoreShapeId(sensors.beginEvents[index].sensorShapeId)}, .visitor = {this, b2StoreShapeId(sensors.beginEvents[index].visitorShapeId)}});
    }
    for (int index = 0; index < sensors.endCount; ++index) {
        sensorEnds.push_back({.sensor = {this, b2StoreShapeId(sensors.endEvents[index].sensorShapeId)}, .visitor = {this, b2StoreShapeId(sensors.endEvents[index].visitorShapeId)}});
    }
    countObjects();
}

void World::checkTransforms(std::span<const Body> bodies, std::size_t valueCount) const {
    if (valueCount < bodies.size() * 3) {
        throw std::invalid_argument("Body transforms take three floats for each body.");
    }
    for (const Body& body : bodies) {
        if (body.getWorld() != this || !body.isValid()) {
            throw std::invalid_argument("Body transforms need live bodies of this world.");
        }
    }
}

void World::readTransforms(std::span<const Body> bodies, std::span<float> values) const {
    checkTransforms(bodies, values.size());
    for (std::size_t index = 0; index < bodies.size(); ++index) {
        const b2Transform transform = b2Body_GetTransform(b2LoadBodyId(bodies[index].getId()));
        const math::Vec2 position = Box2DConverter::toPixels(transform.p, pixelsPerMeter);
        values[index * 3] = position.x;
        values[index * 3 + 1] = position.y;
        values[index * 3 + 2] = b2Rot_GetAngle(transform.q);
    }
}

void World::writeTransforms(std::span<const Body> bodies, std::span<const float> values) {
    checkTransforms(bodies, values.size());
    for (std::size_t index = 0; index < bodies.size(); ++index) {
        const b2Vec2 position = Box2DConverter::toMeters({values[index * 3], values[index * 3 + 1]}, pixelsPerMeter);
        b2Body_SetTransform(b2LoadBodyId(bodies[index].getId()), position, b2MakeRot(values[index * 3 + 2]));
    }
}

void World::setGravity(math::Vec2 value) {
    b2World_SetGravity(b2LoadWorldId(handle), Box2DConverter::toMeters(value, pixelsPerMeter));
}

math::Vec2 World::getGravity() const {
    return Box2DConverter::toPixels(b2World_GetGravity(b2LoadWorldId(handle)), pixelsPerMeter);
}

std::optional<RaycastHit> World::raycast(math::Vec2 from, math::Vec2 to, const CollisionFilter& filter) const {
    return Raycaster(*this).castRay(from, to, {.collision = filter});
}

std::vector<Shape> World::overlapBounds(const math::Rect& area, const CollisionFilter& filter) const {
    struct Query {
        World* world;
        std::vector<Shape> shapes;
    } query{const_cast<World*>(this), {}};

    const b2AABB box{Box2DConverter::toMeters(area.getMin(), pixelsPerMeter), Box2DConverter::toMeters(area.getMax(), pixelsPerMeter)};
    // clang-format off
    b2World_OverlapAABB(b2LoadWorldId(handle), box, Box2DConverter::toQueryFilter(filter), [](b2ShapeId shape, void* context) {
        auto* found = static_cast<Query*>(context);
        found->shapes.emplace_back(found->world, b2StoreShapeId(shape));
        return true;
    }, &query);
    // clang-format on
    return std::move(query.shapes);
}

std::vector<Shape> World::queryRect(const math::Rect& area, const CollisionFilter& filter) const {
    return overlapBounds(area, filter);
}

std::vector<Shape> World::queryCircle(math::Vec2 center, float radius, const CollisionFilter& filter) const {
    const b2Vec2 middle = Box2DConverter::toMeters(center, pixelsPerMeter);
    const float reach = radius / pixelsPerMeter;
    const b2ShapeProxy proxy = b2MakeProxy(&middle, 1, reach);

    struct Query {
        World* world;
        std::vector<Shape> shapes;
    } query{const_cast<World*>(this), {}};

    // clang-format off
    b2World_OverlapShape(b2LoadWorldId(handle), &proxy, Box2DConverter::toQueryFilter(filter), [](b2ShapeId shape, void* context) {
        auto* found = static_cast<Query*>(context);
        found->shapes.emplace_back(found->world, b2StoreShapeId(shape));
        return true;
    }, &query);
    // clang-format on
    return std::move(query.shapes);
}

std::vector<Shape> World::queryPoint(math::Vec2 point, const CollisionFilter& filter) const {
    const b2Vec2 location = Box2DConverter::toMeters(point, pixelsPerMeter);
    std::vector<Shape> shapes = overlapBounds({point.x, point.y, 0.0F, 0.0F}, filter);
    std::erase_if(shapes, [location](const Shape& shape) { return !b2Shape_TestPoint(b2LoadShapeId(shape.getId()), location); });
    return shapes;
}

void World::debugDraw(graphics2d::Renderer& renderer, const graphics2d::DrawOrder& order) const {
    DebugDraw(renderer, order, pixelsPerMeter).draw(b2LoadWorldId(handle));
}

std::size_t World::getBodyCount() const {
    return static_cast<std::size_t>(b2World_GetCounters(b2LoadWorldId(handle)).bodyCount);
}

} // namespace haylen::physics2d
