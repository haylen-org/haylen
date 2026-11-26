#include "3d/physics/jolt/Simulation.hpp"

#include <chrono>

#include "3d/physics/jolt/StateHasher.hpp"

namespace haylen::physics3d::jolt {

// Jolt picks the number of mutexes of the bodies itself when it receives none.
Simulation::Simulation(const WorldSettings& settings, core::JobSystem* jobSystem) : allocator(static_cast<std::size_t>(settings.temporaryMemory) * kMegabyte), jobs(std::make_shared<StepJobs>(jobSystem, settings.threads)) {
    const auto maxBodies = static_cast<JPH::uint>(settings.maxBodies);
    const auto maxBodyPairs = static_cast<JPH::uint>(settings.maxBodyPairs);
    const auto maxContactConstraints = static_cast<JPH::uint>(settings.maxContactConstraints);
    system.Init(maxBodies, 0, maxBodyPairs, maxContactConstraints, layers.getBroadPhaseLayers(), layers.getBroadPhaseFilter(), layers.getPairFilter());
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
    system.SetGravity(JPH::Vec3(settings.gravity.x, settings.gravity.y, settings.gravity.z));
    collisionSteps = settings.collisionSteps;
}

Simulation::StepReport Simulation::step(float deltaSeconds) {
    const bool parallel = system.GetNumActiveBodies(JPH::EBodyType::RigidBody) >= kParallelBodies;
    jobs->setParallel(parallel);
    stepThreads = jobs->GetMaxConcurrency();

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

} // namespace haylen::physics3d::jolt
