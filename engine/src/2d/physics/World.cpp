#include "haylen/2d/physics/World.hpp"

#include <box2d/box2d.h>

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "2d/physics/Box2DConverter.hpp"
#include "2d/physics/DebugDraw.hpp"
#include "2d/physics/StepTasks.hpp"
#include "haylen/2d/physics/Raycaster.hpp"
#include "haylen/math/Math.hpp"

namespace haylen::physics2d {

const World::Settings World::kDefaultSettings{};
const float World::kRevoluteLimit = 0.99F * math::Math::kPi;
debug::ObjectCounter& World::bodyCounter = *new debug::ObjectCounter("PhysicsBody", debug::ObjectCounter::Kind::Native);
debug::ObjectCounter& World::contactCounter = *new debug::ObjectCounter("PhysicsContact", debug::ObjectCounter::Kind::Native);

// Decides in the pre-solve callback of Box2D whether a contact with a one-way platform holds. It may run on several threads at once, so it only reads the platforms and dropping bodies, which change between steps, and the velocities of bodies, which stay still while Box2D collides.
class World::OneWayFilter final {
  public:
    static bool preSolve(b2ShapeId shapeA, b2ShapeId shapeB, b2Manifold* manifold, void* context) {
        const auto& world = *static_cast<const World*>(context);
        const math::Vec2 normal{manifold->normal.x, manifold->normal.y};

        // The manifold normal points from the first shape to the second.
        return holds(world, shapeA, shapeB, normal, *manifold) && holds(world, shapeB, shapeA, -normal, *manifold);
    }

  private:
    // Returns whether a contact whose normal points from the platform to the other shape holds. It holds when the normal leans toward the open side of the platform and the other shape was outside the platform at the start of the step, which the separation and the speed between them tell.
    static bool holds(const World& world, b2ShapeId platform, b2ShapeId other, math::Vec2 normal, const b2Manifold& manifold) {
        const auto found = world.oneWayShapes.find(b2StoreShapeId(platform));
        if (found == world.oneWayShapes.end()) {
            return true;
        }
        const b2BodyId otherBody = b2Shape_GetBody(other);
        if (world.droppingBodies.contains(b2StoreBodyId(otherBody)) || math::Vec2::dot(normal, found->second.worldDirection) < kOneWayThreshold) {
            return false;
        }

        float separation = 0.0F;
        b2Vec2 point = manifold.points[0].point;
        for (int index = 0; index < manifold.pointCount; ++index) {
            if (manifold.points[index].separation < separation) {
                separation = manifold.points[index].separation;
                point = manifold.points[index].point;
            }
        }
        if (separation >= -kOneWayDepth) {
            return true;
        }

        const b2Vec2 relative = b2Sub(b2Body_GetWorldPointVelocity(otherBody, point), b2Body_GetWorldPointVelocity(b2Shape_GetBody(platform), point));
        const float approach = -(relative.x * normal.x + relative.y * normal.y);
        return separation + approach * world.lastStep >= -kOneWayDepth;
    }
};

World::World(const Settings& settings, core::JobSystem* jobSystem) : pixelsPerMeter(settings.pixelsPerMeter), subSteps(settings.subSteps), threads(settings.threads), interpolate(settings.interpolate), jobs(jobSystem) {
    if (settings.pixelsPerMeter <= 0.0F || settings.subSteps < 1 || settings.threads < 1) {
        throw std::invalid_argument("A physics world needs positive pixels per meter, at least one sub-step and at least one thread.");
    }
    if (settings.threads > 1 && jobSystem == nullptr) {
        throw std::invalid_argument("A physics world with more than one thread needs a job system.");
    }

    b2WorldDef def = b2DefaultWorldDef();
    def.gravity = Box2DConverter::toMeters(settings.gravity, pixelsPerMeter);
    def.enableContinuous = settings.continuous;
    def.enableSleep = settings.sleepEnabled;
    def.contactHertz = settings.contactHertz;
    def.contactDampingRatio = settings.contactDampingRatio;
    if (settings.contactPushSpeed) {
        def.maxContactPushSpeed = *settings.contactPushSpeed / pixelsPerMeter;
    }
    if (settings.maxSpeed) {
        def.maximumLinearSpeed = *settings.maxSpeed / pixelsPerMeter;
    }
    if (settings.restitutionThreshold) {
        def.restitutionThreshold = *settings.restitutionThreshold / pixelsPerMeter;
    }
    if (settings.hitThreshold) {
        def.hitEventThreshold = *settings.hitThreshold / pixelsPerMeter;
    }
    contactHertz = def.contactHertz;
    contactDampingRatio = def.contactDampingRatio;
    contactPushSpeed = def.maxContactPushSpeed * pixelsPerMeter;

#if defined(__EMSCRIPTEN__)
    // The web runs everything on one thread.
    threads = 1;
#else
    if (threads > 1) {
        threads = std::min(threads, kMaxThreads);
        tasks = std::make_unique<StepTasks>(*jobSystem, threads);
        def.workerCount = threads;
        def.enqueueTask = &StepTasks::enqueue;
        def.finishTask = &StepTasks::finish;
        def.userTaskContext = tasks.get();
    }
#endif

    const b2WorldId world = b2CreateWorld(&def);
    if (!b2World_IsValid(world)) {
        throw std::runtime_error("Too many physics worlds exist at once to create another one.");
    }
    handle = b2StoreWorldId(world);
    b2World_SetPreSolveCallback(world, &OneWayFilter::preSolve, this);
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
    def.linearDamping = Box2DConverter::toDamping(options.linearDamping);
    def.angularDamping = Box2DConverter::toDamping(options.angularDamping);
    def.gravityScale = options.gravityScale;
    def.fixedRotation = options.fixedRotation;
    def.isBullet = options.bullet;
    def.allowFastRotation = options.fastRotation;
    def.enableSleep = options.sleepEnabled;
    if (options.sleepThreshold) {
        def.sleepThreshold = Box2DConverter::toSpeed(*options.sleepThreshold, pixelsPerMeter);
    }
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
    checkLimits(type, options);

    const b2WorldId world = b2LoadWorldId(handle);
    const b2BodyId a = b2LoadBodyId(first.getId());
    const b2BodyId b = b2LoadBodyId(second.getId());
    const b2Vec2 anchorA = Box2DConverter::toMeters(options.anchorA, pixelsPerMeter);
    const b2Vec2 anchorB = Box2DConverter::toMeters(options.anchorB, pixelsPerMeter);
    const float relativeAngle = b2RelativeAngle(b2Body_GetRotation(b), b2Body_GetRotation(a));
    const float maxForce = options.maxMotorForce / pixelsPerMeter;
    const float maxTorque = options.maxMotorTorque / (pixelsPerMeter * pixelsPerMeter);
    b2JointId joint{};

    switch (type) {
    case Joint::Type::Distance: {
        const float length = options.length > 0.0F ? options.length / pixelsPerMeter : b2Distance(anchorA, anchorB);
        if (!(length >= Box2DConverter::kLinearSlop)) {
            throw std::invalid_argument("A distance joint needs a length of at least 0.005 meters.");
        }
        b2DistanceJointDef def = b2DefaultDistanceJointDef();
        def.bodyIdA = a;
        def.bodyIdB = b;
        def.localAnchorA = b2Body_GetLocalPoint(a, anchorA);
        def.localAnchorB = b2Body_GetLocalPoint(b, anchorB);
        def.length = length;
        def.enableSpring = options.enableSpring;
        def.hertz = options.hertz;
        def.dampingRatio = options.dampingRatio;
        def.enableLimit = options.enableLimit;
        def.minLength = std::max(options.lower / pixelsPerMeter, Box2DConverter::kLinearSlop);
        def.maxLength = std::max(options.upper / pixelsPerMeter, def.minLength);
        def.enableMotor = options.enableMotor;
        def.motorSpeed = options.motorSpeed / pixelsPerMeter;
        def.maxMotorForce = maxForce;
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
        def.referenceAngle = relativeAngle;
        def.enableLimit = options.enableLimit;
        def.lowerAngle = options.lower;
        def.upperAngle = options.upper;
        def.enableMotor = options.enableMotor;
        def.motorSpeed = options.motorSpeed;
        def.maxMotorTorque = maxTorque;
        def.enableSpring = options.enableSpring;
        def.hertz = options.hertz;
        def.dampingRatio = options.dampingRatio;
        def.targetAngle = options.targetAngle;
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
        def.referenceAngle = relativeAngle;
        def.enableLimit = options.enableLimit;
        def.lowerTranslation = options.lower / pixelsPerMeter;
        def.upperTranslation = options.upper / pixelsPerMeter;
        def.enableMotor = options.enableMotor;
        def.motorSpeed = options.motorSpeed / pixelsPerMeter;
        def.maxMotorForce = maxForce;
        def.enableSpring = options.enableSpring;
        def.hertz = options.hertz;
        def.dampingRatio = options.dampingRatio;
        def.targetTranslation = options.targetTranslation / pixelsPerMeter;
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
        def.referenceAngle = relativeAngle;
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
        def.angularOffset = relativeAngle;
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

    Joint created{this, b2StoreJointId(joint)};
    created.setBreakForce(options.breakForce);
    created.setBreakTorque(options.breakTorque);
    return created;
}

// Box2D rejects limits it cannot hold even while they are disabled.
void World::checkLimits(Joint::Type type, const Joint::Options& options) {
    const bool ordered = options.lower <= options.upper;
    if (type == Joint::Type::Revolute && (!ordered || options.lower < -kRevoluteLimit || options.upper > kRevoluteLimit)) {
        throw std::invalid_argument("A revolute joint needs a lower limit that is not above the upper one, both within 0.99 pi radians of zero.");
    }
    if ((type == Joint::Type::Prismatic || type == Joint::Type::Wheel || type == Joint::Type::Distance) && !ordered) {
        throw std::invalid_argument("A prismatic, wheel or distance joint needs a lower limit that is not above the upper one.");
    }
    if ((options.breakForce && !(*options.breakForce >= 0.0F)) || (options.breakTorque && !(*options.breakTorque >= 0.0F))) {
        throw std::invalid_argument("A joint needs a break force and a break torque of zero or more.");
    }
}

std::uint64_t World::addStepHook(StepPhase phase, StepHook hook) {
    const std::uint64_t id = nextHook++;
    hooks.push_back({.id = id, .phase = phase, .function = std::move(hook)});
    return id;
}

void World::removeStepHook(std::uint64_t id) {
    std::erase_if(hooks, [id](const Hook& hook) { return hook.id == id; });
}

void World::runHooks(StepPhase phase, float deltaSeconds) {
    for (const Hook& hook : hooks) {
        if (hook.phase == phase) {
            hook.function(deltaSeconds);
        }
    }
}

// Turns the direction of every one-way platform with its body before the step, so the pre-solve callback reads it without touching Box2D, and counts down the bodies that drop through platforms.
void World::prepareOneWayPlatforms(float deltaSeconds) {
    for (auto platform = oneWayShapes.begin(); platform != oneWayShapes.end();) {
        const b2ShapeId shape = b2LoadShapeId(platform->first);
        if (!b2Shape_IsValid(shape)) {
            platform = oneWayShapes.erase(platform);
            continue;
        }
        const b2Vec2 direction = b2RotateVector(b2Body_GetRotation(b2Shape_GetBody(shape)), {platform->second.direction.x, platform->second.direction.y});
        platform->second.worldDirection = {direction.x, direction.y};
        ++platform;
    }
    std::erase_if(droppingBodies, [](const auto& body) { return body.second <= 0.0F || !b2Body_IsValid(b2LoadBodyId(body.first)); });
    for (auto& [id, seconds] : droppingBodies) {
        seconds -= deltaSeconds;
    }
}

bool World::isDroppingThrough(const Body& body) const {
    return droppingBodies.contains(body.getId());
}

void World::step(float deltaSeconds) {
    const b2WorldId world = b2LoadWorldId(handle);
    const auto hooksStart = std::chrono::steady_clock::now();
    runHooks(StepPhase::Before, deltaSeconds);
    auto hookTime = std::chrono::steady_clock::now() - hooksStart;

    prepareOneWayPlatforms(deltaSeconds);
    lastStep = deltaSeconds;
    if (tasks) {
        tasks->setParallel(b2World_GetAwakeBodyCount(world) >= kParallelBodies);
    }
    b2World_Step(world, deltaSeconds, subSteps);

    events.contactsRead = false;
    events.sensorsRead = false;
    jointBreaks.clear();
    breakJoints();
    if (interpolate) {
        recordMotion();
    }
    countObjects();

    const auto afterStart = std::chrono::steady_clock::now();
    runHooks(StepPhase::After, deltaSeconds);
    hookTime += std::chrono::steady_clock::now() - afterStart;
    hookMilliseconds = std::chrono::duration<float, std::milli>(hookTime).count();
}

// Destroys the joints whose force or torque passed their break limits in this step, in the order of their ids.
void World::breakJoints() {
    std::erase_if(breakLimits, [](const auto& limit) { return !b2Joint_IsValid(b2LoadJointId(limit.first)); });
    for (auto limit = breakLimits.begin(); limit != breakLimits.end();) {
        const b2JointId joint = b2LoadJointId(limit->first);
        const b2Vec2 force = b2Joint_GetConstraintForce(joint);
        const float torque = b2Joint_GetConstraintTorque(joint);
        if ((!limit->second.force || b2Length(force) <= *limit->second.force) && (!limit->second.torque || std::abs(torque) <= *limit->second.torque)) {
            ++limit;
            continue;
        }
        jointBreaks.push_back({
            .joint = {this, limit->first},
            .first = {this, b2StoreBodyId(b2Joint_GetBodyA(joint))},
            .second = {this, b2StoreBodyId(b2Joint_GetBodyB(joint))},
            .force = Box2DConverter::toPixels(force, pixelsPerMeter),
            .torque = torque * pixelsPerMeter * pixelsPerMeter,
        });
        b2DestroyJoint(joint);
        limit = breakLimits.erase(limit);
    }
}

// Keeps the transforms of the last two steps of every body that moved, indexed by the slot of the body.
void World::recordMotion() {
    ++motionStep;
    const b2BodyEvents moves = b2World_GetBodyEvents(b2LoadWorldId(handle));
    for (int index = 0; index < moves.moveCount; ++index) {
        const b2BodyMoveEvent& move = moves.moveEvents[index];
        const auto slot = static_cast<std::size_t>(move.bodyId.index1 - 1);
        if (slot >= motions.size()) {
            motions.resize(slot + 1);
        }
        Motion& motion = motions[slot];
        const std::uint64_t id = b2StoreBodyId(move.bodyId);
        const Transform current{Box2DConverter::toPixels(move.transform.p, pixelsPerMeter), b2Rot_GetAngle(move.transform.q)};
        motion.previous = motion.id == id ? motion.current : current;
        motion.current = current;
        motion.id = id;
        motion.step = motionStep;
    }
}

World::Transform World::blendMotion(const Body& body, float blend) const {
    const b2Transform transform = b2Body_GetTransform(b2LoadBodyId(body.getId()));
    const Transform actual{Box2DConverter::toPixels(transform.p, pixelsPerMeter), b2Rot_GetAngle(transform.q)};
    const auto slot = static_cast<std::size_t>(b2LoadBodyId(body.getId()).index1 - 1);
    if (slot >= motions.size()) {
        return actual;
    }
    const Motion& motion = motions[slot];
    if (motion.id != body.getId() || motion.step != motionStep || motion.current.position != actual.position || motion.current.rotation != actual.rotation) {
        return actual;
    }
    const float turn = std::remainder(motion.current.rotation - motion.previous.rotation, math::Math::kTau);
    return {math::Vec2::lerp(motion.previous.position, motion.current.position, blend), motion.previous.rotation + turn * blend};
}

World::Transform World::getInterpolatedTransform(const Body& body, float blend) const {
    if (!interpolate) {
        throw std::logic_error("Only a world created with interpolation blends the transforms of its bodies.");
    }
    if (body.getWorld() != this || !body.isValid()) {
        throw std::invalid_argument("Body transforms need live bodies of this world.");
    }
    return blendMotion(body, std::clamp(blend, 0.0F, 1.0F));
}

void World::readContactEvents() const {
    if (events.contactsRead) {
        return;
    }
    events.contactsRead = true;
    events.contactBegins.clear();
    events.contactEnds.clear();
    events.contactHits.clear();

    auto* self = const_cast<World*>(this);
    const b2ContactEvents contacts = b2World_GetContactEvents(b2LoadWorldId(handle));
    for (int index = 0; index < contacts.beginCount; ++index) {
        const b2ContactBeginTouchEvent& event = contacts.beginEvents[index];
        ContactEvent begin{.first = {self, b2StoreShapeId(event.shapeIdA)}, .second = {self, b2StoreShapeId(event.shapeIdB)}, .normal = {event.manifold.normal.x, event.manifold.normal.y}};
        if (event.manifold.pointCount > 0) {
            begin.point = Box2DConverter::toPixels(event.manifold.points[0].point, pixelsPerMeter);
        }
        events.contactBegins.push_back(begin);
    }
    for (int index = 0; index < contacts.endCount; ++index) {
        const b2ContactEndTouchEvent& event = contacts.endEvents[index];
        events.contactEnds.push_back({.first = {self, b2StoreShapeId(event.shapeIdA)}, .second = {self, b2StoreShapeId(event.shapeIdB)}});
    }
    for (int index = 0; index < contacts.hitCount; ++index) {
        const b2ContactHitEvent& event = contacts.hitEvents[index];
        events.contactHits.push_back({.first = {self, b2StoreShapeId(event.shapeIdA)}, .second = {self, b2StoreShapeId(event.shapeIdB)}, .point = Box2DConverter::toPixels(event.point, pixelsPerMeter), .normal = {event.normal.x, event.normal.y}, .speed = event.approachSpeed * pixelsPerMeter});
    }
}

void World::readSensorEvents() const {
    if (events.sensorsRead) {
        return;
    }
    events.sensorsRead = true;
    events.sensorBegins.clear();
    events.sensorEnds.clear();

    auto* self = const_cast<World*>(this);
    const b2SensorEvents sensors = b2World_GetSensorEvents(b2LoadWorldId(handle));
    for (int index = 0; index < sensors.beginCount; ++index) {
        events.sensorBegins.push_back({.sensor = {self, b2StoreShapeId(sensors.beginEvents[index].sensorShapeId)}, .visitor = {self, b2StoreShapeId(sensors.beginEvents[index].visitorShapeId)}});
    }
    for (int index = 0; index < sensors.endCount; ++index) {
        events.sensorEnds.push_back({.sensor = {self, b2StoreShapeId(sensors.endEvents[index].sensorShapeId)}, .visitor = {self, b2StoreShapeId(sensors.endEvents[index].visitorShapeId)}});
    }
}

const std::vector<ContactEvent>& World::getContactBegins() const {
    readContactEvents();
    return events.contactBegins;
}

const std::vector<ContactEvent>& World::getContactEnds() const {
    readContactEvents();
    return events.contactEnds;
}

const std::vector<ContactEvent>& World::getContactHits() const {
    readContactEvents();
    return events.contactHits;
}

const std::vector<SensorEvent>& World::getSensorBegins() const {
    readSensorEvents();
    return events.sensorBegins;
}

const std::vector<SensorEvent>& World::getSensorEnds() const {
    readSensorEvents();
    return events.sensorEnds;
}

void World::takeDestroyedBodies(std::vector<std::uint64_t>& ids) {
    ids.insert(ids.end(), destroyedBodies.begin(), destroyedBodies.end());
    destroyedBodies.clear();
}

void World::forgetBody(std::uint64_t id) {
    destroyedBodies.push_back(id);
    droppingBodies.erase(id);
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

void World::readTransforms(std::span<const Body> bodies, std::span<float> values, std::optional<float> blend) const {
    checkTransforms(bodies, values.size());
    if (blend && !interpolate) {
        throw std::logic_error("Only a world created with interpolation blends the transforms of its bodies.");
    }
    for (std::size_t index = 0; index < bodies.size(); ++index) {
        Transform transform;
        if (blend) {
            transform = blendMotion(bodies[index], std::clamp(*blend, 0.0F, 1.0F));
        } else {
            const b2Transform current = b2Body_GetTransform(b2LoadBodyId(bodies[index].getId()));
            transform = {Box2DConverter::toPixels(current.p, pixelsPerMeter), b2Rot_GetAngle(current.q)};
        }
        values[index * 3] = transform.position.x;
        values[index * 3 + 1] = transform.position.y;
        values[index * 3 + 2] = transform.rotation;
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
    wakeAll();
}

math::Vec2 World::getGravity() const {
    return Box2DConverter::toPixels(b2World_GetGravity(b2LoadWorldId(handle)), pixelsPerMeter);
}

// Box2D wakes every sleeping island when sleeping turns off.
void World::wakeAll() {
    const b2WorldId world = b2LoadWorldId(handle);
    if (b2World_IsSleepingEnabled(world)) {
        b2World_EnableSleeping(world, false);
        b2World_EnableSleeping(world, true);
    }
}

void World::setSubSteps(int value) {
    if (value < 1) {
        throw std::invalid_argument("A physics world needs at least one sub-step.");
    }
    subSteps = value;
}

void World::setContinuousEnabled(bool value) {
    b2World_EnableContinuous(b2LoadWorldId(handle), value);
}

bool World::isContinuousEnabled() const {
    return b2World_IsContinuousEnabled(b2LoadWorldId(handle));
}

void World::setSleepEnabled(bool value) {
    b2World_EnableSleeping(b2LoadWorldId(handle), value);
}

bool World::isSleepEnabled() const {
    return b2World_IsSleepingEnabled(b2LoadWorldId(handle));
}

void World::setMaxSpeed(float value) {
    b2World_SetMaximumLinearSpeed(b2LoadWorldId(handle), Box2DConverter::toSpeed(value, pixelsPerMeter));
}

float World::getMaxSpeed() const {
    return b2World_GetMaximumLinearSpeed(b2LoadWorldId(handle)) * pixelsPerMeter;
}

void World::applyContactTuning() {
    b2World_SetContactTuning(b2LoadWorldId(handle), contactHertz, contactDampingRatio, contactPushSpeed / pixelsPerMeter);
}

void World::setContactHertz(float value) {
    contactHertz = Box2DConverter::toPositive(value, "A physics world needs a positive contact stiffness.");
    applyContactTuning();
}

void World::setContactDampingRatio(float value) {
    contactDampingRatio = Box2DConverter::toPositive(value, "A physics world needs a positive contact damping ratio.");
    applyContactTuning();
}

void World::setContactPushSpeed(float value) {
    contactPushSpeed = Box2DConverter::toSpeed(value, pixelsPerMeter) * pixelsPerMeter;
    applyContactTuning();
}

void World::setRestitutionThreshold(float value) {
    b2World_SetRestitutionThreshold(b2LoadWorldId(handle), Box2DConverter::toSpeed(value, pixelsPerMeter));
}

float World::getRestitutionThreshold() const {
    return b2World_GetRestitutionThreshold(b2LoadWorldId(handle)) * pixelsPerMeter;
}

void World::setHitThreshold(float value) {
    b2World_SetHitEventThreshold(b2LoadWorldId(handle), Box2DConverter::toSpeed(value, pixelsPerMeter));
}

float World::getHitThreshold() const {
    return b2World_GetHitEventThreshold(b2LoadWorldId(handle)) * pixelsPerMeter;
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
    if (!(area.width >= 0.0F) || !(area.height >= 0.0F)) {
        throw std::invalid_argument("A rectangle query needs a width and height of zero or more.");
    }
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

std::vector<Shape> World::pick(math::Vec2 point, float radius, const CollisionFilter& filter) const {
    const b2Vec2 location = Box2DConverter::toMeters(point, pixelsPerMeter);
    std::vector<std::pair<float, Shape>> found;
    for (const Shape& shape : queryCircle(point, std::max(radius, 0.0F), filter)) {
        const b2ShapeId id = b2LoadShapeId(shape.getId());
        const float distance = b2Shape_TestPoint(id, location) ? 0.0F : b2Distance(b2Shape_GetClosestPoint(id, location), location);
        found.emplace_back(distance, shape);
    }
    std::stable_sort(found.begin(), found.end(), [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });

    std::vector<Shape> shapes;
    shapes.reserve(found.size());
    for (const auto& [distance, shape] : found) {
        shapes.push_back(shape);
    }
    return shapes;
}

std::uint64_t World::computeStateHash(std::span<const Body> bodies) const {
    std::uint64_t hash = 14695981039346656037ULL;
    // clang-format off
    const auto mix = [&hash](float value) {
        hash ^= std::bit_cast<std::uint32_t>(value);
        hash *= 1099511628211ULL;
    };
    // clang-format on
    for (const Body& body : bodies) {
        if (body.getWorld() != this || !body.isValid()) {
            throw std::invalid_argument("A state hash needs live bodies of this world.");
        }
        const b2BodyId id = b2LoadBodyId(body.getId());
        const b2Transform transform = b2Body_GetTransform(id);
        const b2Vec2 velocity = b2Body_GetLinearVelocity(id);
        for (const float value : {transform.p.x, transform.p.y, transform.q.c, transform.q.s, velocity.x, velocity.y, b2Body_GetAngularVelocity(id)}) {
            mix(value);
        }
    }
    return hash;
}

void World::debugDraw(graphics2d::Renderer& renderer, const graphics2d::DrawOrder& order) const {
    DebugDraw(renderer, order, pixelsPerMeter).draw(b2LoadWorldId(handle));
}

std::size_t World::getBodyCount() const {
    return static_cast<std::size_t>(b2World_GetCounters(b2LoadWorldId(handle)).bodyCount);
}

std::size_t World::getAwakeBodyCount() const {
    return static_cast<std::size_t>(b2World_GetAwakeBodyCount(b2LoadWorldId(handle)));
}

World::Stats World::getStats() const {
    const b2WorldId world = b2LoadWorldId(handle);
    const b2Counters counters = b2World_GetCounters(world);
    const b2Profile profile = b2World_GetProfile(world);
    return {
        .bodies = counters.bodyCount,
        .awakeBodies = b2World_GetAwakeBodyCount(world),
        .shapes = counters.shapeCount,
        .contacts = counters.contactCount,
        .joints = counters.jointCount,
        .islands = counters.islandCount,
        .stepMilliseconds = profile.step,
        .collideMilliseconds = profile.collide,
        .solveMilliseconds = profile.solve,
        .continuousMilliseconds = profile.bullets,
        .sleepMilliseconds = profile.sleepIslands,
        .hookMilliseconds = hookMilliseconds,
    };
}

} // namespace haylen::physics2d
