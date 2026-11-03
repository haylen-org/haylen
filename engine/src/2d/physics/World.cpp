#include "haylen/2d/physics/World.hpp"

#include <box2d/box2d.h>

#include <algorithm>
#include <bit>
#include <cfloat>
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

// Decides in the pre-solve callback of Box2D whether a contact with a one-way platform holds, and in its pre-continuous callback whether a fast body stops on one. They may run on several threads at once, so they only read the platforms, the dropping bodies and the standing contacts, which change between steps, and the bodies, which stay still while Box2D collides.
class World::OneWayFilter final {
  public:
    // The manifold normal points from the first shape to the second, and a contact that does not hold loses its points.
    static void preSolve(b2ShapeId shapeA, b2ShapeId shapeB, b2Manifold* manifold, void* context) {
        const auto& world = *static_cast<const World*>(context);
        const math::Vec2 normal{manifold->normal.x, manifold->normal.y};
        const Touch touch = deepest(shapeA, *manifold);
        if (!holds(world, shapeA, shapeB, normal, touch) || !holds(world, shapeB, shapeA, -normal, touch)) {
            manifold->pointCount = 0;
        }
    }

    // A fast body reaches the surface it hits, so it stands on a platform it falls on from the open side.
    static bool preContinuous(b2ShapeId shapeA, b2ShapeId shapeB, b2Pos point, b2Vec2 normal, void* context) {
        const auto& world = *static_cast<const World*>(context);
        const Touch touch{.point = point, .separation = 0.0F};
        return holds(world, shapeA, shapeB, {normal.x, normal.y}, touch) && holds(world, shapeB, shapeA, {-normal.x, -normal.y}, touch);
    }

    // Lists the shapes that touch a one-way platform after a step, which only those the platform held do, sorted for the lookups of the next step.
    void collectStanding(const World& world) {
        standing.clear();
        bodies.clear();
        for (const auto& [shape, platform] : world.oneWayShapes) {
            const std::uint64_t body = b2StoreBodyId(b2Shape_GetBody(b2LoadShapeId(shape)));
            if (std::find(bodies.begin(), bodies.end(), body) == bodies.end()) {
                bodies.push_back(body);
            }
        }
        for (const std::uint64_t body : bodies) {
            const b2BodyId id = b2LoadBodyId(body);
            contacts.resize(static_cast<std::size_t>(b2Body_GetContactCapacity(id)));
            const int count = b2Body_GetContactData(id, contacts.data(), static_cast<int>(contacts.size()));
            for (int index = 0; index < count; ++index) {
                const std::uint64_t first = b2StoreShapeId(contacts[static_cast<std::size_t>(index)].shapeIdA);
                const std::uint64_t second = b2StoreShapeId(contacts[static_cast<std::size_t>(index)].shapeIdB);
                if (world.oneWayShapes.contains(first)) {
                    standing.emplace_back(first, second);
                }
                if (world.oneWayShapes.contains(second)) {
                    standing.emplace_back(second, first);
                }
            }
        }
        std::sort(standing.begin(), standing.end());
        standing.erase(std::unique(standing.begin(), standing.end()), standing.end());
    }

  private:
    // The deepest point of a contact in world space and how far the shapes overlap there, negative while they do.
    struct Touch {
        b2Vec2 point{};
        float separation = 0.0F;
    };

    // Box2D measures the points of a manifold it has not solved yet from the origin of the body of the first shape.
    [[nodiscard]] static Touch deepest(b2ShapeId shapeA, const b2Manifold& manifold) {
        Touch touch{.point = manifold.points[0].anchorA, .separation = 0.0F};
        for (int index = 0; index < manifold.pointCount; ++index) {
            if (manifold.points[index].separation < touch.separation) {
                touch = {.point = manifold.points[index].anchorA, .separation = manifold.points[index].separation};
            }
        }
        touch.point = b2Add(b2Body_GetPosition(b2Shape_GetBody(shapeA)), touch.point);
        return touch;
    }

    // Returns whether a contact whose normal points from the platform to the other shape holds. It holds when the normal leans toward the open side of the platform and the other shape stood on the platform in the last step or was outside it at the start of this one, which the separation and the speed between them tell.
    static bool holds(const World& world, b2ShapeId platform, b2ShapeId other, math::Vec2 normal, const Touch& touch) {
        const auto found = world.oneWayShapes.find(b2StoreShapeId(platform));
        if (found == world.oneWayShapes.end()) {
            return true;
        }
        const b2BodyId otherBody = b2Shape_GetBody(other);
        if (world.droppingBodies.contains(b2StoreBodyId(otherBody)) || math::Vec2::dot(normal, found->second.worldDirection) < kOneWayThreshold) {
            return false;
        }
        if (std::binary_search(world.oneWayFilter->standing.begin(), world.oneWayFilter->standing.end(), std::pair{b2StoreShapeId(platform), b2StoreShapeId(other)})) {
            return true;
        }
        if (touch.separation >= -kOneWayDepth) {
            return true;
        }

        const b2Vec2 relative = b2Sub(b2Body_GetWorldPointVelocity(otherBody, touch.point), b2Body_GetWorldPointVelocity(b2Shape_GetBody(platform), touch.point));
        const float approach = -(relative.x * normal.x + relative.y * normal.y);
        return touch.separation + approach * world.lastStep >= -kOneWayDepth;
    }

    std::vector<std::pair<std::uint64_t, std::uint64_t>> standing;
    std::vector<std::uint64_t> bodies;
    std::vector<b2ContactData> contacts;
};

World::World(const Settings& settings, core::JobSystem* jobSystem) : pixelsPerMeter(settings.pixelsPerMeter), subSteps(settings.subSteps), threads(settings.threads), interpolate(settings.interpolate), jobs(jobSystem), oneWayFilter(std::make_unique<OneWayFilter>()) {
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
        def.contactSpeed = *settings.contactPushSpeed / pixelsPerMeter;
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
    contactPushSpeed = def.contactSpeed * pixelsPerMeter;

#if defined(__EMSCRIPTEN__)
    // The web runs everything on one thread.
    threads = 1;
#else
    // Box2D steps a world without task callbacks on threads of its own, so a world of one thread gives none and Box2D steps it on the calling thread.
    if (threads > 1) {
        threads = std::min(threads, kMaxThreads);
        tasks = std::make_unique<StepTasks>(*jobSystem);
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
    b2World_SetPreSolveCallback(world, &OneWayFilter::preSolve, &OneWayFilter::preContinuous, this);
}

World::~World() {
    destroy();
}

void World::destroy() {
    if (isDestroyed()) {
        return;
    }
    b2DestroyWorld(b2LoadWorldId(handle));
    handle = 0;

    hooks.clear();
    events = {};
    jointBreaks.clear();
    destroyedBodies.clear();
    mouseJoints.clear();
    chainTies.clear();
    motions.clear();
    oneWayShapes.clear();
    droppingBodies.clear();
    fastBodies.clear();
    liveBodies.set(0);
    liveContacts.set(0);
}

std::uint32_t World::getHandle() const {
    checkLive();
    return handle;
}

void World::checkLive() const {
    if (isDestroyed()) {
        throw std::logic_error("The physics world was destroyed.");
    }
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
    def.motionLocks = {.linearX = options.locks.x, .linearY = options.locks.y, .angularZ = options.locks.rotation};
    def.isBullet = options.bullet;
    def.safetyFactor = Box2DConverter::toContinuousThreshold(options.continuousThreshold);
    def.allowFastRotation = options.fastRotation;
    def.enableSleep = options.sleepEnabled;
    if (options.sleepThreshold) {
        def.sleepThreshold = Box2DConverter::toSpeed(*options.sleepThreshold, pixelsPerMeter);
    }
    const Body body{this, b2StoreBodyId(b2CreateBody(b2LoadWorldId(getHandle()), &def))};
    if (options.fastRotation) {
        fastBodies.insert(body.getId());
    }
    countObjects();
    return body;
}

void World::countObjects() {
    const b2Counters counters = b2World_GetCounters(b2LoadWorldId(getHandle()));
    liveBodies.set(static_cast<std::size_t>(counters.bodyCount));
    liveContacts.set(static_cast<std::size_t>(counters.contactCount));
}

Joint World::createJoint(Joint::Type type, Body first, Body second, const Joint::Options& options) {
    if (!first.isValid() || !second.isValid() || first.getWorld() != this || second.getWorld() != this) {
        throw std::invalid_argument("A joint needs two bodies of this world.");
    }
    checkLimits(type, options);

    const b2WorldId world = b2LoadWorldId(getHandle());
    const b2BodyId a = b2LoadBodyId(first.getId());
    const b2BodyId b = b2LoadBodyId(second.getId());
    const b2Vec2 anchorA = Box2DConverter::toMeters(options.anchorA, pixelsPerMeter);
    const b2Vec2 anchorB = Box2DConverter::toMeters(options.anchorB, pixelsPerMeter);
    const float maxForce = options.maxMotorForce / pixelsPerMeter;
    const float maxTorque = options.maxMotorTorque / (pixelsPerMeter * pixelsPerMeter);

    // Both frames sit on the anchor, turned along the axis in the world, so every joint measures its angle from the pose the bodies have now, and the x axis of the first frame is the slide axis of prismatic and wheel joints.
    const math::Vec2 direction = type == Joint::Type::Prismatic || type == Joint::Type::Wheel ? options.axis.getNormalized() : math::Vec2{1.0F, 0.0F};
    const b2Vec2 axis = b2Body_GetLocalVector(a, {direction.x, direction.y});
    b2JointDef base = b2DefaultFilterJointDef().base;
    base.bodyIdA = a;
    base.bodyIdB = b;
    base.localFrameA = {.p = b2Body_GetLocalPoint(a, anchorA), .q = {.c = axis.x, .s = axis.y}};
    base.localFrameB = {.p = b2Body_GetLocalPoint(b, anchorA), .q = b2InvMulRot(b2Body_GetRotation(b), b2MulRot(b2Body_GetRotation(a), base.localFrameA.q))};
    base.forceThreshold = options.breakForce ? *options.breakForce / pixelsPerMeter : FLT_MAX;
    base.torqueThreshold = options.breakTorque ? *options.breakTorque / (pixelsPerMeter * pixelsPerMeter) : FLT_MAX;
    base.collideConnected = options.collideConnected;
    b2JointId joint{};

    switch (type) {
    case Joint::Type::Distance: {
        const float length = options.length > 0.0F ? options.length / pixelsPerMeter : b2Distance(anchorA, anchorB);
        if (!(length >= Box2DConverter::kLinearSlop)) {
            throw std::invalid_argument("A distance joint needs a length of at least 0.005 meters.");
        }
        b2DistanceJointDef def = b2DefaultDistanceJointDef();
        def.base = base;
        def.base.localFrameB.p = b2Body_GetLocalPoint(b, anchorB);
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
        joint = b2CreateDistanceJoint(world, &def);
        break;
    }
    case Joint::Type::Revolute: {
        b2RevoluteJointDef def = b2DefaultRevoluteJointDef();
        def.base = base;
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
        joint = b2CreateRevoluteJoint(world, &def);
        break;
    }
    case Joint::Type::Prismatic: {
        b2PrismaticJointDef def = b2DefaultPrismaticJointDef();
        def.base = base;
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
        joint = b2CreatePrismaticJoint(world, &def);
        break;
    }
    case Joint::Type::Weld: {
        b2WeldJointDef def = b2DefaultWeldJointDef();
        def.base = base;
        def.linearHertz = options.hertz;
        def.angularHertz = options.hertz;
        def.linearDampingRatio = options.dampingRatio;
        def.angularDampingRatio = options.dampingRatio;
        joint = b2CreateWeldJoint(world, &def);
        break;
    }
    case Joint::Type::Wheel: {
        b2WheelJointDef def = b2DefaultWheelJointDef();
        def.base = base;
        def.enableSpring = options.enableSpring;
        def.hertz = options.hertz;
        def.dampingRatio = options.dampingRatio;
        def.enableLimit = options.enableLimit;
        def.lowerTranslation = options.lower / pixelsPerMeter;
        def.upperTranslation = options.upper / pixelsPerMeter;
        def.enableMotor = options.enableMotor;
        def.motorSpeed = options.motorSpeed;
        def.maxMotorTorque = maxTorque;
        joint = b2CreateWheelJoint(world, &def);
        break;
    }
    case Joint::Type::Mouse: {
        // A mouse joint is a motor joint whose spring pulls the grabbed point of the second body toward a target held by the first, and whose turning friction keeps the body from spinning around the point.
        b2MotorJointDef def = b2DefaultMotorJointDef();
        def.base = base;
        def.base.localFrameA = {.p = b2Body_GetLocalPoint(a, anchorB), .q = b2Rot_identity};
        def.base.localFrameB = {.p = b2Body_GetLocalPoint(b, anchorB), .q = b2Rot_identity};
        def.linearHertz = options.hertz > 0.0F ? options.hertz : kMouseHertz;
        def.linearDampingRatio = options.dampingRatio > 0.0F ? options.dampingRatio : kMouseDampingRatio;
        def.maxSpringForce = maxForce > 0.0F ? maxForce : kMouseAcceleration * b2Body_GetMass(b);
        def.maxVelocityTorque = getMouseTurnFriction(second.getId(), def.maxSpringForce);
        joint = b2CreateMotorJoint(world, &def);
        mouseJoints.insert(b2StoreJointId(joint));
        break;
    }
    case Joint::Type::Motor: {
        // The spring of a motor joint pulls the origin of the second body toward the offset it has now in the space of the first.
        b2MotorJointDef def = b2DefaultMotorJointDef();
        def.base = base;
        def.base.localFrameA = {.p = b2Body_GetLocalPoint(a, b2Body_GetPosition(b)), .q = b2InvMulRot(b2Body_GetRotation(a), b2Body_GetRotation(b))};
        def.base.localFrameB = b2Transform_identity;
        def.linearHertz = options.hertz > 0.0F ? options.hertz : kMotorHertz;
        def.angularHertz = def.linearHertz;
        def.linearDampingRatio = options.dampingRatio > 0.0F ? options.dampingRatio : kMotorDampingRatio;
        def.angularDampingRatio = def.linearDampingRatio;
        def.maxSpringForce = maxForce;
        def.maxSpringTorque = maxTorque;
        joint = b2CreateMotorJoint(world, &def);
        break;
    }
    case Joint::Type::Filter: {
        b2FilterJointDef def = b2DefaultFilterJointDef();
        def.base.bodyIdA = a;
        def.base.bodyIdB = b;
        joint = b2CreateFilterJoint(world, &def);
        break;
    }
    }
    return {this, b2StoreJointId(joint)};
}

// Returns the most torque, in Box2D units, with which a mouse joint keeps its body from spinning around the grabbed point: a share of its pull at the radius of gyration of the body, the distance at which the mass of the body turns as it does.
float World::getMouseTurnFriction(std::uint64_t body, float force) {
    const b2MassData mass = b2Body_GetMassData(b2LoadBodyId(body));
    return mass.mass > 0.0F ? kMouseTurnFriction * std::sqrt(mass.rotationalInertia / mass.mass) * force : 0.0F;
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
    if ((type == Joint::Type::Prismatic || type == Joint::Type::Wheel) && options.axis.isZero()) {
        throw std::invalid_argument("A prismatic or wheel joint needs an axis that is not zero.");
    }
    if ((options.breakForce && !(*options.breakForce >= 0.0F)) || (options.breakTorque && !(*options.breakTorque >= 0.0F))) {
        throw std::invalid_argument("A joint needs a break force and a break torque of zero or more.");
    }
}

std::uint64_t World::addStepHook(StepPhase phase, StepHook hook) {
    checkLive();
    const std::uint64_t id = nextHook++;
    hooks.push_back({.id = id, .phase = phase, .function = std::move(hook)});
    return id;
}

void World::removeStepHook(std::uint64_t id) {
    std::erase_if(hooks, [id](const Hook& hook) { return hook.id == id; });
}

void World::tieToChain(const Joint& joint, std::span<const Joint> chain) {
    ChainTie& tie = chainTies.emplace_back();
    tie.joint = joint.getId();
    for (const Joint& link : chain) {
        tie.chain.push_back(link.getId());
    }
}

// Destroys the joints whose chains lost a link since the last step, so the pieces of a cut rope fall apart.
void World::releaseBrokenChains() {
    // clang-format off
    std::erase_if(chainTies, [](const ChainTie& tie) {
        const b2JointId joint = b2LoadJointId(tie.joint);
        if (!b2Joint_IsValid(joint)) {
            return true;
        }
        if (std::all_of(tie.chain.begin(), tie.chain.end(), [](std::uint64_t link) { return b2Joint_IsValid(b2LoadJointId(link)); })) {
            return false;
        }
        b2DestroyJoint(joint);
        return true;
    });
    // clang-format on
}

void World::runHooks(StepPhase phase, float deltaSeconds) {
    for (const Hook& hook : hooks) {
        if (hook.phase == phase) {
            hook.function(deltaSeconds);
        }
    }
}

// Turns the direction of every one-way platform with its body before the step, so the pre-solve callback reads it without touching Box2D, lists the shapes that stand on the platforms and counts down the bodies that drop through platforms.
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
    oneWayFilter->collectStanding(*this);
    std::erase_if(droppingBodies, [](const auto& body) { return body.second <= 0.0F || !b2Body_IsValid(b2LoadBodyId(body.first)); });
    for (auto& [id, seconds] : droppingBodies) {
        seconds -= deltaSeconds;
    }
}

bool World::isDroppingThrough(const Body& body) const {
    return droppingBodies.contains(body.getId());
}

void World::step(float deltaSeconds) {
    const b2WorldId world = b2LoadWorldId(getHandle());
    releaseBrokenChains();
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

// Destroys the joints whose force or torque passed their break limits in a sub-step of this step, which Box2D reports in the order of their ids, and forgets the mouse joints that are gone.
void World::breakJoints() {
    const b2JointEvents loaded = b2World_GetJointEvents(b2LoadWorldId(getHandle()));
    for (int index = 0; index < loaded.count; ++index) {
        const b2JointId joint = loaded.jointEvents[index].jointId;
        if (!b2Joint_IsValid(joint)) {
            continue;
        }
        jointBreaks.push_back({
            .joint = {this, b2StoreJointId(joint)},
            .first = {this, b2StoreBodyId(b2Joint_GetBodyA(joint))},
            .second = {this, b2StoreBodyId(b2Joint_GetBodyB(joint))},
            .force = Box2DConverter::toPixels(b2Joint_GetConstraintForce(joint), pixelsPerMeter),
            .torque = b2Joint_GetConstraintTorque(joint) * pixelsPerMeter * pixelsPerMeter,
        });
        b2DestroyJoint(joint);
    }
    std::erase_if(mouseJoints, [](std::uint64_t joint) { return !b2Joint_IsValid(b2LoadJointId(joint)); });
}

// Keeps the transforms of the last two steps of every body that moved, indexed by the slot of the body.
void World::recordMotion() {
    ++motionStep;
    const b2BodyEvents moves = b2World_GetBodyEvents(b2LoadWorldId(getHandle()));
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
    const b2ContactEvents contacts = b2World_GetContactEvents(b2LoadWorldId(getHandle()));
    const float subStep = lastStep / static_cast<float>(subSteps);
    for (int index = 0; index < contacts.beginCount; ++index) {
        const b2ContactBeginTouchEvent& event = contacts.beginEvents[index];
        events.contactBegins.push_back({.first = {self, b2StoreShapeId(event.shapeIdA)}, .second = {self, b2StoreShapeId(event.shapeIdB)}, .manifold = Box2DConverter::toSolvedManifold(event.contactId, pixelsPerMeter, subStep)});
    }
    for (int index = 0; index < contacts.endCount; ++index) {
        const b2ContactEndTouchEvent& event = contacts.endEvents[index];
        events.contactEnds.push_back({.first = {self, b2StoreShapeId(event.shapeIdA)}, .second = {self, b2StoreShapeId(event.shapeIdB)}});
    }

    // A hit whose contact ended within the step keeps the point and normal Box2D recorded for it.
    for (int index = 0; index < contacts.hitCount; ++index) {
        const b2ContactHitEvent& event = contacts.hitEvents[index];
        ContactManifold hit = Box2DConverter::toSolvedManifold(event.contactId, pixelsPerMeter, subStep);
        if (hit.pointCount == 0) {
            hit.pointCount = 1;
            hit.points[0].point = Box2DConverter::toPixels(event.point, pixelsPerMeter);
            hit.normal = {event.normal.x, event.normal.y};
        }
        hit.approachSpeed = event.approachSpeed * pixelsPerMeter;
        events.contactHits.push_back({.first = {self, b2StoreShapeId(event.shapeIdA)}, .second = {self, b2StoreShapeId(event.shapeIdB)}, .manifold = hit});
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
    const b2SensorEvents sensors = b2World_GetSensorEvents(b2LoadWorldId(getHandle()));
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
    fastBodies.erase(id);
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
    b2World_SetGravity(b2LoadWorldId(getHandle()), Box2DConverter::toMeters(value, pixelsPerMeter));
    wakeAll();
}

math::Vec2 World::getGravity() const {
    return Box2DConverter::toPixels(b2World_GetGravity(b2LoadWorldId(getHandle())), pixelsPerMeter);
}

// Box2D wakes every sleeping island when sleeping turns off.
void World::wakeAll() {
    const b2WorldId world = b2LoadWorldId(getHandle());
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
    b2World_EnableContinuous(b2LoadWorldId(getHandle()), value);
}

bool World::isContinuousEnabled() const {
    return b2World_IsContinuousEnabled(b2LoadWorldId(getHandle()));
}

void World::setSleepEnabled(bool value) {
    b2World_EnableSleeping(b2LoadWorldId(getHandle()), value);
}

bool World::isSleepEnabled() const {
    return b2World_IsSleepingEnabled(b2LoadWorldId(getHandle()));
}

void World::setMaxSpeed(float value) {
    b2World_SetMaxLinearSpeed(b2LoadWorldId(getHandle()), Box2DConverter::toSpeed(value, pixelsPerMeter));
}

float World::getMaxSpeed() const {
    return b2World_GetMaxLinearSpeed(b2LoadWorldId(getHandle())) * pixelsPerMeter;
}

void World::applyContactTuning() {
    b2World_SetContactTuning(b2LoadWorldId(getHandle()), contactHertz, contactDampingRatio, contactPushSpeed / pixelsPerMeter);
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
    b2World_SetRestitutionThreshold(b2LoadWorldId(getHandle()), Box2DConverter::toSpeed(value, pixelsPerMeter));
}

float World::getRestitutionThreshold() const {
    return b2World_GetRestitutionThreshold(b2LoadWorldId(getHandle())) * pixelsPerMeter;
}

void World::setHitThreshold(float value) {
    b2World_SetHitEventThreshold(b2LoadWorldId(getHandle()), Box2DConverter::toSpeed(value, pixelsPerMeter));
}

float World::getHitThreshold() const {
    return b2World_GetHitEventThreshold(b2LoadWorldId(getHandle())) * pixelsPerMeter;
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
    b2World_OverlapAABB(b2LoadWorldId(getHandle()), b2Pos_zero, box, Box2DConverter::toQueryFilter(filter), [](b2ShapeId shape, void* context) {
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
    b2World_OverlapShape(b2LoadWorldId(getHandle()), b2Pos_zero, &proxy, Box2DConverter::toQueryFilter(filter), [](b2ShapeId shape, void* context) {
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
    DebugDraw(renderer, order, pixelsPerMeter).draw(b2LoadWorldId(getHandle()));
}

std::size_t World::getBodyCount() const {
    return static_cast<std::size_t>(b2World_GetCounters(b2LoadWorldId(getHandle())).bodyCount);
}

std::size_t World::getAwakeBodyCount() const {
    return static_cast<std::size_t>(b2World_GetAwakeBodyCount(b2LoadWorldId(getHandle())));
}

World::Stats World::getStats() const {
    const b2WorldId world = b2LoadWorldId(getHandle());
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
