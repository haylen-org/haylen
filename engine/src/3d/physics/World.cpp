#include "haylen/3d/physics/World.hpp"

#include <cmath>
#include <stdexcept>

#include "haylen/core/Log.hpp"

namespace haylen::physics3d {

World::World(const WorldSettings& initial, Backend& backend, core::JobSystem* jobSystem) : settings(initial), backendName(backend.getName()) {
    settings.validate();
    if (settings.threads > 1 && jobSystem == nullptr) {
        throw std::invalid_argument("A 3D physics world that steps on more than one thread needs the job system of the engine.");
    }

    simulation = backend.createSimulation(settings, jobSystem);
}

World::~World() = default;

void World::destroy() noexcept {
    simulation.reset();
}

Simulation& World::getSimulation() const {
    if (simulation == nullptr) {
        throw std::logic_error("The 3D physics world was destroyed.");
    }

    return *simulation;
}

void World::step(float deltaSeconds) {
    Simulation& simulated = getSimulation();
    if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0F) {
        throw std::invalid_argument("A step of a 3D physics world takes a time of zero seconds or more.");
    }

    const Simulation::StepReport report = simulated.step(deltaSeconds);
    lastStep = deltaSeconds;
    reportLimits(report);
}

// A full cache leaves contacts out of the step, so bodies may sink into each other, and full temporary memory makes the step allocate, so each is a limit of the settings that the developer raises.
void World::reportLimits(const Simulation::StepReport& report) {
    if (report.bodyPairsFull && !reported.bodyPairsFull) {
        reported.bodyPairsFull = true;
        core::Log::warning("A step of a 3D physics world found more pairs of touching bodies than \"maxBodyPairs\" holds, so it left contacts out. Raise \"maxBodyPairs\" in the \"physics3d\" section of \"haylen.json\" or in the options of the world.");
    }

    if (report.contactConstraintsFull && !reported.contactConstraintsFull) {
        reported.contactConstraintsFull = true;
        core::Log::warning("A step of a 3D physics world found more contacts than \"maxContactConstraints\" holds, so it left contacts out. Raise \"maxContactConstraints\" in the \"physics3d\" section of \"haylen.json\" or in the options of the world.");
    }

    if (report.temporaryMemoryFull && !reported.temporaryMemoryFull) {
        reported.temporaryMemoryFull = true;
        core::Log::warning("A step of a 3D physics world needed more than the {} megabytes of \"temporaryMemory\", so it allocated the rest, which costs time. Raise \"temporaryMemory\" in the \"physics3d\" section of \"haylen.json\" or in the options of the world.", settings.temporaryMemory);
    }
}

void World::reconfigure(const WorldSettings& changed) {
    Simulation& simulated = getSimulation();
    changed.validate();
    simulated.configure(changed);
    settings = changed;
}

void World::setGravity(math::Vec3 value) {
    WorldSettings changed = settings;
    changed.gravity = value;
    reconfigure(changed);
}

void World::setCollisionSteps(int value) {
    WorldSettings changed = settings;
    changed.collisionSteps = value;
    reconfigure(changed);
}

void World::setVelocitySteps(int value) {
    WorldSettings changed = settings;
    changed.velocitySteps = value;
    reconfigure(changed);
}

void World::setPositionSteps(int value) {
    WorldSettings changed = settings;
    changed.positionSteps = value;
    reconfigure(changed);
}

void World::setSleepEnabled(bool value) {
    WorldSettings changed = settings;
    changed.sleepEnabled = value;
    reconfigure(changed);
}

void World::setTimeBeforeSleep(float value) {
    WorldSettings changed = settings;
    changed.timeBeforeSleep = value;
    reconfigure(changed);
}

void World::setSleepVelocity(float value) {
    WorldSettings changed = settings;
    changed.sleepVelocity = value;
    reconfigure(changed);
}

void World::setSpeculativeContactDistance(float value) {
    WorldSettings changed = settings;
    changed.speculativeContactDistance = value;
    reconfigure(changed);
}

void World::setPenetrationSlop(float value) {
    WorldSettings changed = settings;
    changed.penetrationSlop = value;
    reconfigure(changed);
}

void World::setRestitutionThreshold(float value) {
    WorldSettings changed = settings;
    changed.restitutionThreshold = value;
    reconfigure(changed);
}

WorldStats World::getStats() const {
    return getSimulation().getStats();
}

std::uint64_t World::computeStateHash() const {
    return getSimulation().computeStateHash();
}

} // namespace haylen::physics3d
