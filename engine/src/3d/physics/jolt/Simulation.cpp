#include "3d/physics/jolt/Simulation.hpp"

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLockInterface.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>

#include <array>
#include <chrono>
#include <stdexcept>
#include <utility>

#include "3d/physics/jolt/Converter.hpp"
#include "3d/physics/jolt/Materials.hpp"
#include "3d/physics/jolt/Shape.hpp"
#include "3d/physics/jolt/StateHasher.hpp"

namespace haylen::physics3d::jolt {

// Jolt picks the number of mutexes of the bodies itself when it receives none.
Simulation::Simulation(const WorldSettings& settings, core::JobSystem* jobSystem) : allocator(static_cast<std::size_t>(settings.temporaryMemory) * kMegabyte), jobs(std::make_shared<StepJobs>(jobSystem, settings.threads)) {
    const auto maxBodies = static_cast<JPH::uint>(settings.maxBodies);
    const auto maxBodyPairs = static_cast<JPH::uint>(settings.maxBodyPairs);
    const auto maxContactConstraints = static_cast<JPH::uint>(settings.maxContactConstraints);
    system.Init(maxBodies, 0, maxBodyPairs, maxContactConstraints, layers.getBroadPhaseLayers(), layers.getBroadPhaseFilter(), layers.getPairFilter());
    Materials::install(system);
    system.SetContactListener(&contacts);
    configure(settings);
}

Simulation::~Simulation() = default;

void Simulation::configure(const WorldSettings& settings) {
    JPH::PhysicsSettings tuning = system.GetPhysicsSettings();
    tuning.mNumVelocitySteps = static_cast<JPH::uint>(settings.velocitySteps);
    tuning.mNumPositionSteps = static_cast<JPH::uint>(settings.positionSteps);
    tuning.mAllowSleeping = settings.sleepEnabled;
    tuning.mTimeBeforeSleep = settings.timeBeforeSleep;
    tuning.mPointVelocitySleepThreshold = settings.sleepVelocity;
    tuning.mSpeculativeContactDistance = settings.speculativeContactDistance;
    tuning.mPenetrationSlop = settings.penetrationSlop;
    tuning.mMinVelocityForRestitution = settings.restitutionThreshold;
    system.SetPhysicsSettings(tuning);
    system.SetGravity(Converter::toJolt(settings.gravity));
    collisionSteps = settings.collisionSteps;
}

Simulation::StepReport Simulation::step(float deltaSeconds) {
    const bool parallel = system.GetNumActiveBodies(JPH::EBodyType::RigidBody) >= kParallelBodies;
    jobs->setParallel(parallel);
    stepThreads = jobs->GetMaxConcurrency();

    subStep = deltaSeconds / static_cast<float>(collisionSteps);
    const auto start = std::chrono::steady_clock::now();
    const JPH::EPhysicsUpdateError errors = system.Update(deltaSeconds, collisionSteps, &allocator, jobs.get());
    stepMilliseconds = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - start).count();

    const auto has = [errors](JPH::EPhysicsUpdateError error) { return (errors & error) != JPH::EPhysicsUpdateError::None; };
    return {
        .bodyPairsFull = has(JPH::EPhysicsUpdateError::BodyPairCacheFull),
        .contactConstraintsFull = has(JPH::EPhysicsUpdateError::ManifoldCacheFull) || has(JPH::EPhysicsUpdateError::ContactConstraintsFull),
        .temporaryMemoryFull = allocator.takeOverflow(),
    };
}

WorldStats Simulation::getStats() const {
    return {
        .bodies = static_cast<int>(system.GetNumBodies()),
        .activeBodies = static_cast<int>(system.GetNumActiveBodies(JPH::EBodyType::RigidBody) + system.GetNumActiveBodies(JPH::EBodyType::SoftBody)),
        .stepThreads = stepThreads,
        .stepMilliseconds = stepMilliseconds,
        .temporaryMemoryPeak = allocator.getPeak(),
    };
}

std::uint64_t Simulation::computeStateHash() const {
    StateHasher hasher;
    system.SaveState(hasher);
    return hasher.getHash();
}

JPH::EMotionType Simulation::toJolt(Body::Type value) noexcept {
    switch (value) {
    case Body::Type::Static:
        return JPH::EMotionType::Static;
    case Body::Type::Kinematic:
        return JPH::EMotionType::Kinematic;
    case Body::Type::Dynamic:
        break;
    }

    return JPH::EMotionType::Dynamic;
}

JPH::EAllowedDOFs Simulation::toJolt(const Body::MotionLocks& value) noexcept {
    const std::array<std::pair<bool, JPH::EAllowedDOFs>, 6> axes{{
        {value.x, JPH::EAllowedDOFs::TranslationX},
        {value.y, JPH::EAllowedDOFs::TranslationY},
        {value.z, JPH::EAllowedDOFs::TranslationZ},
        {value.rotationX, JPH::EAllowedDOFs::RotationX},
        {value.rotationY, JPH::EAllowedDOFs::RotationY},
        {value.rotationZ, JPH::EAllowedDOFs::RotationZ},
    }};

    JPH::EAllowedDOFs allowed = JPH::EAllowedDOFs::None;
    for (const auto& [locked, axis] : axes) {
        if (!locked) {
            allowed = allowed | axis;
        }
    }

    return allowed;
}

Body::MotionLocks Simulation::fromJolt(JPH::EAllowedDOFs value) noexcept {
    const auto locked = [value](JPH::EAllowedDOFs axis) { return (value & axis) == JPH::EAllowedDOFs::None; };
    return {
        .x = locked(JPH::EAllowedDOFs::TranslationX),
        .y = locked(JPH::EAllowedDOFs::TranslationY),
        .z = locked(JPH::EAllowedDOFs::TranslationZ),
        .rotationX = locked(JPH::EAllowedDOFs::RotationX),
        .rotationY = locked(JPH::EAllowedDOFs::RotationY),
        .rotationZ = locked(JPH::EAllowedDOFs::RotationZ),
    };
}

JPH::Body& Simulation::getBody(BodyId id) const {
    return *system.GetBodyLockInterfaceNoLock().TryGetBody(JPH::BodyID(id));
}

Simulation::Record& Simulation::getRecord(BodyId id) {
    return records[getBodySlot(id)];
}

const Simulation::Record& Simulation::getRecord(BodyId id) const {
    return records[getBodySlot(id)];
}

std::size_t Simulation::getBodySlot(BodyId id) const noexcept {
    return JPH::BodyID(id).GetIndex();
}

bool Simulation::isBodyValid(BodyId id) const noexcept {
    return system.GetBodyLockInterfaceNoLock().TryGetBody(JPH::BodyID(id)) != nullptr;
}

// A static body moves nothing it lands on, so it wakes nothing.
JPH::EActivation Simulation::activationOf(BodyId id) const {
    return getBody(id).IsStatic() ? JPH::EActivation::DontActivate : JPH::EActivation::Activate;
}

void Simulation::takeShape(Record& record, const std::shared_ptr<const physics3d::Shape>& shape) const {
    record.compound = nullptr;
    if (shape->getKind() != physics3d::Shape::Kind::MutableCompound) {
        record.shape = shape;
        return;
    }

    const auto& source = dynamic_cast<const JPH::MutableCompoundShape&>(Shape::of(*shape));
    record.compound = source.Clone();
    record.shape = std::make_shared<const Shape>(physics3d::Shape::Kind::MutableCompound, JPH::RefConst<JPH::Shape>(record.compound.GetPtr()), shape->isSolid());
}

JPH::RefConst<JPH::Shape> Simulation::getJoltShape(const Record& record) const {
    const JPH::RefConst<JPH::Shape> base = record.compound != nullptr ? JPH::RefConst<JPH::Shape>(record.compound.GetPtr()) : JPH::RefConst<JPH::Shape>(&Shape::of(*record.shape));
    if (!record.centerOfMass) {
        return base;
    }

    const JPH::Vec3 offset = Converter::toJolt(*record.centerOfMass) - base->GetCenterOfMass();
    JPH::OffsetCenterOfMassShapeSettings settings(offset, base);
    settings.SetEmbedded();
    return settings.Create().Get();
}

// A shape without a volume has no mass, and only static and kinematic bodies take one, which never use their mass, so their motion takes the mass that was set or a mass of one kilogram that Jolt needs.
JPH::MassProperties Simulation::getMass(const Record& record, const JPH::Shape& shape) {
    if (!Shape::isSolid(shape)) {
        JPH::MassProperties properties;
        properties.mMass = record.mass.value_or(1.0F);
        properties.mInertia = JPH::Mat44::sScale(record.inertia ? Converter::toJolt(*record.inertia) : JPH::Vec3::sReplicate(properties.mMass));
        return properties;
    }

    JPH::MassProperties properties = shape.GetMassProperties();
    if (record.mass) {
        properties.ScaleToMass(*record.mass);
    }

    if (record.inertia) {
        properties.mInertia = JPH::Mat44::sScale(Converter::toJolt(*record.inertia));
    }

    return properties;
}

// A change to how a body moves wakes it, so a sleeping body reacts to it at once.
void Simulation::wake(BodyId id) {
    const JPH::Body& body = getBody(id);
    if (!body.IsStatic() && body.IsInBroadPhase()) {
        system.GetBodyInterfaceNoLock().ActivateBody(body.GetID());
    }
}

void Simulation::applyMass(BodyId id) {
    JPH::Body& body = getBody(id);
    if (!body.IsDynamic()) {
        return;
    }

    JPH::MotionProperties& motion = *body.GetMotionProperties();
    motion.SetMassProperties(motion.GetAllowedDOFs(), getMass(getRecord(id), *body.GetShape()));
    motion.SetLinearVelocityClamped(motion.GetLinearVelocity());
    motion.SetAngularVelocityClamped(motion.GetAngularVelocity());
    wake(id);
}

void Simulation::applyShape(BodyId id) {
    system.GetBodyInterfaceNoLock().SetShape(JPH::BodyID(id), getJoltShape(getRecord(id)), false, activationOf(id));
    applyMass(id);
}

std::optional<Simulation::BodyId> Simulation::createBody(const Body::Options& options) {
    Record record;
    takeShape(record, options.shape);
    record.mass = options.mass;
    record.inertia = options.inertia;
    record.centerOfMass = options.centerOfMass;

    const JPH::RefConst<JPH::Shape> shape = getJoltShape(record);
    const bool moving = options.type != Body::Type::Static;
    JPH::BodyCreationSettings creation(shape, Converter::toJolt(options.position), Converter::toJolt(options.rotation), toJolt(options.type), layers.getLayer(options.filter, partOf(options.type, options.sensor)));
    creation.mAllowDynamicOrKinematic = true;
    creation.mLinearVelocity = Converter::toJolt(options.linearVelocity);
    creation.mAngularVelocity = Converter::toJolt(options.angularVelocity);
    creation.mMotionQuality = options.motionQuality == Body::MotionQuality::LinearCast ? JPH::EMotionQuality::LinearCast : JPH::EMotionQuality::Discrete;
    creation.mAllowedDOFs = toJolt(options.locks);
    creation.mIsSensor = options.sensor;
    creation.mCollideKinematicVsNonDynamic = options.collideWithStatic;
    creation.mEnhancedInternalEdgeRemoval = options.enhancedInternalEdgeRemoval.value_or(false);
    creation.mAllowSleeping = options.sleepEnabled;
    creation.mFriction = options.friction;
    creation.mRestitution = options.restitution;
    creation.mLinearDamping = options.linearDamping;
    creation.mAngularDamping = options.angularDamping;
    creation.mMaxLinearVelocity = options.maxLinearSpeed;
    creation.mMaxAngularVelocity = options.maxAngularSpeed;
    creation.mGravityFactor = options.gravityScale;
    creation.mOverrideMassProperties = JPH::EOverrideMassProperties::MassAndInertiaProvided;
    creation.mMassPropertiesOverride = getMass(record, *shape);

    JPH::BodyInterface& interface = system.GetBodyInterfaceNoLock();
    JPH::Body* body = interface.CreateBody(creation);
    if (body == nullptr) {
        return std::nullopt;
    }

    Materials::setRule(*body, Body::Material::Friction, options.frictionCombine);
    Materials::setRule(*body, Body::Material::Restitution, options.restitutionCombine);
    const BodyId id = body->GetID().GetIndexAndSequenceNumber();
    const std::size_t slot = getBodySlot(id);
    if (slot >= records.size()) {
        records.resize(slot + 1);
    }

    records[slot] = std::move(record);
    if (options.enabled) {
        interface.AddBody(body->GetID(), moving ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
    }

    return id;
}

void Simulation::destroyBody(BodyId id) {
    joints.destroyJointsOf(id);
    JPH::BodyInterface& interface = system.GetBodyInterfaceNoLock();
    const JPH::BodyID body(id);
    if (interface.IsAdded(body)) {
        interface.RemoveBody(body);
    }

    interface.DestroyBody(body);
    getRecord(id) = {};
}

Body::Type Simulation::getBodyType(BodyId id) const {
    switch (getBody(id).GetMotionType()) {
    case JPH::EMotionType::Static:
        return Body::Type::Static;
    case JPH::EMotionType::Kinematic:
        return Body::Type::Kinematic;
    case JPH::EMotionType::Dynamic:
        break;
    }

    return Body::Type::Dynamic;
}

// A body that stops moving joins the bodies that never move in the broad phase, and one that starts moving leaves them.
void Simulation::setBodyType(BodyId id, Body::Type value) {
    JPH::BodyInterface& interface = system.GetBodyInterfaceNoLock();
    const JPH::BodyID body(id);
    const bool moving = value != Body::Type::Static;
    if (!moving) {
        interface.SetLinearAndAngularVelocity(body, JPH::Vec3::sZero(), JPH::Vec3::sZero());
    }

    interface.SetMotionType(body, toJolt(value), moving ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
    applyLayer(id, getBodyFilter(id));
    applyMass(id);
}

Body::MotionQuality Simulation::getMotionQuality(BodyId id) const {
    return getBody(id).GetMotionPropertiesUnchecked()->GetMotionQuality() == JPH::EMotionQuality::LinearCast ? Body::MotionQuality::LinearCast : Body::MotionQuality::Discrete;
}

void Simulation::setMotionQuality(BodyId id, Body::MotionQuality value) {
    system.GetBodyInterfaceNoLock().SetMotionQuality(JPH::BodyID(id), value == Body::MotionQuality::LinearCast ? JPH::EMotionQuality::LinearCast : JPH::EMotionQuality::Discrete);
}

Body::MotionLocks Simulation::getMotionLocks(BodyId id) const {
    return fromJolt(getBody(id).GetMotionPropertiesUnchecked()->GetAllowedDOFs());
}

// The locks live with the mass of a body, since a locked axis takes no inertia.
void Simulation::setMotionLocks(BodyId id, const Body::MotionLocks& value) {
    JPH::Body& body = getBody(id);
    JPH::MotionProperties& motion = *body.GetMotionPropertiesUnchecked();
    motion.SetMassProperties(toJolt(value), getMass(getRecord(id), *body.GetShape()));
    motion.SetLinearVelocityClamped(motion.GetLinearVelocity());
    motion.SetAngularVelocityClamped(motion.GetAngularVelocity());
    wake(id);
}

Body::Transform Simulation::getBodyTransform(BodyId id) const {
    const JPH::Body& body = getBody(id);
    return {.position = Converter::fromJolt(body.GetPosition()), .rotation = Converter::fromJolt(body.GetRotation())};
}

void Simulation::setBodyTransform(BodyId id, const Body::Transform& value) {
    system.GetBodyInterfaceNoLock().SetPositionAndRotation(JPH::BodyID(id), Converter::toJolt(value.position), Converter::toJolt(value.rotation), activationOf(id));
}

void Simulation::moveBody(BodyId id, const Body::Transform& target, float seconds) {
    system.GetBodyInterfaceNoLock().MoveKinematic(JPH::BodyID(id), Converter::toJolt(target.position), Converter::toJolt(target.rotation), seconds);
}

void Simulation::readBodyTransforms(std::span<const BodyId> ids, std::span<float> values) const {
    for (std::size_t index = 0; index < ids.size(); ++index) {
        const JPH::Body& body = getBody(ids[index]);
        const JPH::RVec3 position = body.GetPosition();
        const JPH::Quat rotation = body.GetRotation();
        const std::span<float> target = values.subspan(index * 7, 7);
        target[0] = position.GetX();
        target[1] = position.GetY();
        target[2] = position.GetZ();
        target[3] = rotation.GetX();
        target[4] = rotation.GetY();
        target[5] = rotation.GetZ();
        target[6] = rotation.GetW();
    }
}

void Simulation::writeBodyTransforms(std::span<const BodyId> ids, std::span<const float> values) {
    JPH::BodyInterface& interface = system.GetBodyInterfaceNoLock();
    for (std::size_t index = 0; index < ids.size(); ++index) {
        const std::span<const float> source = values.subspan(index * 7, 7);
        const JPH::Quat rotation = JPH::Quat(source[3], source[4], source[5], source[6]).Normalized();
        interface.SetPositionAndRotation(JPH::BodyID(ids[index]), JPH::RVec3(source[0], source[1], source[2]), rotation, activationOf(ids[index]));
    }
}

math::Vec3 Simulation::getBodyVector(BodyId id, Body::Vector which) const {
    const JPH::Body& body = getBody(id);
    switch (which) {
    case Body::Vector::LinearVelocity:
        return Converter::fromJolt(body.GetLinearVelocity());
    case Body::Vector::AngularVelocity:
        return Converter::fromJolt(body.GetAngularVelocity());
    case Body::Vector::CenterOfMass:
        return Converter::fromJolt(body.GetShape()->GetCenterOfMass());
    case Body::Vector::Inertia: {
        const JPH::Mat44 inertia = getMass(getRecord(id), *body.GetShape()).mInertia;
        return {inertia(0, 0), inertia(1, 1), inertia(2, 2)};
    }
    case Body::Vector::WorldCenter:
        break;
    }

    return Converter::fromJolt(body.GetCenterOfMassPosition());
}

void Simulation::setBodyVector(BodyId id, Body::Vector which, math::Vec3 value) {
    JPH::BodyInterface& interface = system.GetBodyInterfaceNoLock();
    switch (which) {
    case Body::Vector::LinearVelocity:
        interface.SetLinearVelocity(JPH::BodyID(id), Converter::toJolt(value));
        return;
    case Body::Vector::AngularVelocity:
        interface.SetAngularVelocity(JPH::BodyID(id), Converter::toJolt(value));
        return;
    case Body::Vector::CenterOfMass:
        getRecord(id).centerOfMass = value;
        applyShape(id);
        return;
    case Body::Vector::Inertia:
        getRecord(id).inertia = value;
        applyMass(id);
        return;
    case Body::Vector::WorldCenter:
        break;
    }
}

float Simulation::getBodyValue(BodyId id, Body::Value which) const {
    const JPH::Body& body = getBody(id);
    const JPH::MotionProperties& motion = *body.GetMotionPropertiesUnchecked();
    switch (which) {
    case Body::Value::Mass:
        return Shape::isSolid(*body.GetShape()) || getRecord(id).mass ? getMass(getRecord(id), *body.GetShape()).mMass : 0.0F;
    case Body::Value::LinearDamping:
        return motion.GetLinearDamping();
    case Body::Value::AngularDamping:
        return motion.GetAngularDamping();
    case Body::Value::GravityScale:
        return motion.GetGravityFactor();
    case Body::Value::MaxLinearSpeed:
        return motion.GetMaxLinearVelocity();
    case Body::Value::MaxAngularSpeed:
        return motion.GetMaxAngularVelocity();
    case Body::Value::Friction:
        return body.GetFriction();
    case Body::Value::Restitution:
        break;
    }

    return body.GetRestitution();
}

void Simulation::setBodyValue(BodyId id, Body::Value which, float value) {
    JPH::Body& body = getBody(id);
    JPH::MotionProperties& motion = *body.GetMotionPropertiesUnchecked();
    switch (which) {
    case Body::Value::Mass:
        getRecord(id).mass = value;
        applyMass(id);
        return;
    case Body::Value::LinearDamping:
        motion.SetLinearDamping(value);
        return;
    case Body::Value::AngularDamping:
        motion.SetAngularDamping(value);
        return;
    case Body::Value::GravityScale:
        motion.SetGravityFactor(value);
        wake(id);
        return;
    case Body::Value::MaxLinearSpeed:
        motion.SetMaxLinearVelocity(value);
        return;
    case Body::Value::MaxAngularSpeed:
        motion.SetMaxAngularVelocity(value);
        return;
    case Body::Value::Friction:
        body.SetFriction(value);
        return;
    case Body::Value::Restitution:
        body.SetRestitution(value);
        return;
    }
}

bool Simulation::getBodyFlag(BodyId id, Body::Flag which) const {
    const JPH::Body& body = getBody(id);
    switch (which) {
    case Body::Flag::SleepEnabled:
        return body.GetAllowSleeping();
    case Body::Flag::Awake:
        return body.IsActive();
    case Body::Flag::Sensor:
        return body.IsSensor();
    case Body::Flag::CollideWithStatic:
        return body.GetCollideKinematicVsNonDynamic();
    case Body::Flag::EnhancedInternalEdgeRemoval:
        return body.GetEnhancedInternalEdgeRemoval();
    case Body::Flag::Enabled:
        break;
    }

    return body.IsInBroadPhase();
}

// Waking or sending to sleep acts only on a moving body in the simulation, since a static body never sleeps nor wakes and a body that is not enabled takes no part.
void Simulation::setBodyFlag(BodyId id, Body::Flag which, bool value) {
    JPH::Body& body = getBody(id);
    JPH::BodyInterface& interface = system.GetBodyInterfaceNoLock();
    switch (which) {
    case Body::Flag::SleepEnabled:
        body.SetAllowSleeping(value);
        return;
    case Body::Flag::Awake:
        if (body.IsStatic() || !body.IsInBroadPhase()) {
            return;
        }

        if (value) {
            interface.ActivateBody(body.GetID());
        } else {
            interface.DeactivateBody(body.GetID());
        }

        return;
    case Body::Flag::Sensor:
        body.SetIsSensor(value);
        applyLayer(id, getBodyFilter(id));
        return;
    case Body::Flag::CollideWithStatic:
        body.SetCollideKinematicVsNonDynamic(value);
        return;
    case Body::Flag::EnhancedInternalEdgeRemoval:
        body.SetEnhancedInternalEdgeRemoval(value);
        return;
    case Body::Flag::Enabled:
        if (value == body.IsInBroadPhase()) {
            return;
        }

        if (value) {
            interface.AddBody(body.GetID(), activationOf(id));
        } else {
            interface.RemoveBody(body.GetID());
        }

        return;
    }
}

Body::CombineRule Simulation::getCombineRule(BodyId id, Body::Material which) const {
    return Materials::getRule(getBody(id), which);
}

void Simulation::setCombineRule(BodyId id, Body::Material which, Body::CombineRule value) {
    Materials::setRule(getBody(id), which, value);
}

void Simulation::resetMass(BodyId id) {
    Record& record = getRecord(id);
    record.mass.reset();
    record.inertia.reset();
    record.centerOfMass.reset();
    applyShape(id);
}

math::Vec3 Simulation::getVelocityAt(BodyId id, math::Vec3 point) const {
    return Converter::fromJolt(getBody(id).GetPointVelocity(Converter::toJolt(point)));
}

void Simulation::pushBody(BodyId id, Body::Push which, math::Vec3 value, std::optional<math::Vec3> point) {
    JPH::BodyInterface& interface = system.GetBodyInterfaceNoLock();
    const JPH::BodyID body(id);
    const JPH::Vec3 amount = Converter::toJolt(value);
    switch (which) {
    case Body::Push::Force:
        if (point) {
            interface.AddForce(body, amount, Converter::toJolt(*point));
        } else {
            interface.AddForce(body, amount);
        }

        return;
    case Body::Push::Impulse:
        if (point) {
            interface.AddImpulse(body, amount, Converter::toJolt(*point));
        } else {
            interface.AddImpulse(body, amount);
        }

        return;
    case Body::Push::Torque:
        interface.AddTorque(body, amount);
        return;
    case Body::Push::AngularImpulse:
        interface.AddAngularImpulse(body, amount);
        return;
    }
}

math::Box Simulation::getBodyBounds(BodyId id) const {
    return Converter::fromJolt(getBody(id).GetWorldSpaceBounds());
}

Layers::Part Simulation::partOf(Body::Type type, bool sensor) noexcept {
    if (sensor) {
        return Layers::Part::Sensor;
    }

    return type == Body::Type::Static ? Layers::Part::Static : Layers::Part::Moving;
}

void Simulation::applyLayer(BodyId id, const CollisionFilter& filter) {
    const JPH::Body& body = getBody(id);
    const JPH::ObjectLayer layer = layers.getLayer(filter, partOf(getBodyType(id), body.IsSensor()));
    system.GetBodyInterfaceNoLock().SetObjectLayer(body.GetID(), layer);
}

CollisionFilter Simulation::getBodyFilter(BodyId id) const {
    return layers.getFilter(getBody(id).GetObjectLayer());
}

void Simulation::setBodyFilter(BodyId id, const CollisionFilter& value) {
    applyLayer(id, value);
    wake(id);
}

std::shared_ptr<const physics3d::Shape> Simulation::getBodyShape(BodyId id) const {
    return getRecord(id).shape;
}

void Simulation::setBodyShape(BodyId id, const std::shared_ptr<const physics3d::Shape>& value) {
    takeShape(getRecord(id), value);
    applyShape(id);
}

std::size_t Simulation::getChildCount(BodyId id) const {
    return getRecord(id).compound->GetNumSubShapes();
}

// Jolt keeps the children of a compound around its center of mass, which moves as children change, so the body keeps where its shape is in the world.
void Simulation::changeCompound(BodyId id, JPH::Vec3 previousCenter) {
    Record& record = getRecord(id);
    record.compound->AdjustCenterOfMass();
    record.shape = std::make_shared<const Shape>(physics3d::Shape::Kind::MutableCompound, JPH::RefConst<JPH::Shape>(record.compound.GetPtr()), Shape::isSolid(*record.compound));
    if (record.centerOfMass) {
        applyShape(id);
        return;
    }

    system.GetBodyInterfaceNoLock().NotifyShapeChanged(JPH::BodyID(id), previousCenter, false, activationOf(id));
    applyMass(id);
}

std::size_t Simulation::addChild(BodyId id, const physics3d::Shape::Child& child) {
    Record& record = getRecord(id);
    const JPH::Vec3 previousCenter = record.compound->GetCenterOfMass();
    const JPH::uint index = record.compound->AddShape(Converter::toJolt(child.position), Converter::toJolt(child.rotation), &Shape::of(*child.shape));
    changeCompound(id, previousCenter);
    return index;
}

void Simulation::removeChild(BodyId id, std::size_t index) {
    Record& record = getRecord(id);
    const JPH::Vec3 previousCenter = record.compound->GetCenterOfMass();
    record.compound->RemoveShape(static_cast<JPH::uint>(index));
    changeCompound(id, previousCenter);
}

void Simulation::setChildTransform(BodyId id, std::size_t index, const Body::Transform& value) {
    Record& record = getRecord(id);
    const JPH::Vec3 previousCenter = record.compound->GetCenterOfMass();
    record.compound->ModifyShape(static_cast<JPH::uint>(index), Converter::toJolt(value.position), Converter::toJolt(value.rotation));
    changeCompound(id, previousCenter);
}

void Simulation::collectActiveBodies(std::vector<BodyId>& ids) const {
    ids.clear();
    const JPH::uint count = system.GetNumActiveBodies(JPH::EBodyType::RigidBody);
    const JPH::BodyID* active = system.GetActiveBodiesUnsafe(JPH::EBodyType::RigidBody);
    for (JPH::uint index = 0; index < count; ++index) {
        ids.push_back(active[index].GetIndexAndSequenceNumber());
    }
}

Simulation::JointId Simulation::createJoint(Joint::Type type, BodyId first, std::optional<BodyId> second, const Joint::Options& options) {
    return joints.create(type, first, second, options);
}

void Simulation::destroyJoint(JointId id) {
    joints.destroy(id);
}

bool Simulation::isJointValid(JointId id) const noexcept {
    return joints.isValid(id);
}

Joint::Type Simulation::getJointType(JointId id) const {
    return joints.getType(id);
}

std::pair<Simulation::BodyId, std::optional<Simulation::BodyId>> Simulation::getJointBodies(JointId id) const {
    return joints.getBodies(id);
}

bool Simulation::isJointEnabled(JointId id) const {
    return joints.isEnabled(id);
}

void Simulation::setJointEnabled(JointId id, bool value) {
    joints.setEnabled(id, value);
}

int Simulation::getJointPriority(JointId id) const {
    return joints.getPriority(id);
}

void Simulation::setJointPriority(JointId id, int value) {
    joints.setPriority(id, value);
}

Joint::Load Simulation::getJointLoad(JointId id) const {
    return joints.getLoad(id, subStep);
}

float Simulation::getJointValue(JointId id, Joint::Value which) const {
    return joints.getValue(id, which);
}

void Simulation::setJointValue(JointId id, Joint::Value which, float value) {
    joints.setValue(id, which, value);
}

void Simulation::setJointLimits(JointId id, float lower, float upper) {
    joints.setLimits(id, lower, upper);
}

Joint::Spring Simulation::getLimitSpring(JointId id) const {
    return joints.getLimitSpring(id);
}

void Simulation::setLimitSpring(JointId id, const Joint::Spring& value) {
    joints.setLimitSpring(id, value);
}

Joint::Motor Simulation::getJointMotor(JointId id, Joint::MotorSlot slot) const {
    return joints.getMotor(id, slot);
}

void Simulation::setJointMotor(JointId id, Joint::MotorSlot slot, const Joint::Motor& value) {
    joints.setMotor(id, slot, value);
}

math::Quat Simulation::getTargetRotation(JointId id) const {
    return joints.getTargetRotation(id);
}

void Simulation::setTargetRotation(JointId id, const math::Quat& value) {
    joints.setTargetRotation(id, value);
}

math::Vec3 Simulation::getTargetAngularVelocity(JointId id) const {
    return joints.getTargetAngularVelocity(id);
}

void Simulation::setTargetAngularVelocity(JointId id, math::Vec3 value) {
    joints.setTargetAngularVelocity(id, value);
}

Joint::AxisSettings Simulation::getJointAxis(JointId id, Joint::Axis axis) const {
    return joints.getAxis(id, axis);
}

void Simulation::setJointAxis(JointId id, Joint::Axis axis, const Joint::AxisSettings& value) {
    joints.setAxis(id, axis, value);
}

} // namespace haylen::physics3d::jolt
