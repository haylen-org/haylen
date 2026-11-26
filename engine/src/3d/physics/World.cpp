#include "haylen/3d/physics/World.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "3d/physics/WorldSlots.hpp"
#include "haylen/core/Log.hpp"

namespace haylen::physics3d {

debug::ObjectCounter& World::bodyCounter = *new debug::ObjectCounter("PhysicsBody3D", debug::ObjectCounter::Kind::Native);

// The slot of the world comes last, once the backend made the simulation, so a world that fails to start holds none.
World::World(const WorldSettings& initial, Backend& backend, core::JobSystem* jobSystem) : settings(initial), backendName(backend.getName()) {
    settings.validate();
    if (settings.threads > 1 && jobSystem == nullptr) {
        throw std::invalid_argument("A 3D physics world that steps on more than one thread needs the job system of the engine.");
    }

    simulation = backend.createSimulation(settings, jobSystem);
    activeBodies.reserve(static_cast<std::size_t>(settings.maxBodies));
    if (settings.interpolate) {
        motions.resize(static_cast<std::size_t>(settings.maxBodies));
    }

    key = WorldSlots::acquire();
}

World::~World() {
    destroy();
}

void World::destroy() noexcept {
    if (simulation == nullptr) {
        return;
    }

    WorldSlots::release(key);
    key = 0;
    hooks.clear();
    simulation.reset();
    bodyCounter.remove(bodyCount);
    bodyCount = 0;
}

Simulation& World::getSimulation() const {
    if (simulation == nullptr) {
        throw std::logic_error("The 3D physics world was destroyed.");
    }

    return *simulation;
}

Body World::createBody(const Body::Options& options) {
    Simulation& simulated = getSimulation();
    options.validate(backendName);

    Body::Options resolved = options;
    resolved.rotation = options.rotation.getNormalized();
    resolved.enhancedInternalEdgeRemoval = options.enhancedInternalEdgeRemoval.value_or(settings.enhancedInternalEdgeRemoval);
    const std::optional<Simulation::BodyId> id = simulated.createBody(resolved);
    if (!id) {
        throw std::length_error("The 3D physics world holds \"maxBodies\" bodies already. Raise \"maxBodies\" in the \"physics3d\" section of \"haylen.json\" or in the options of the world, or destroy bodies it no longer needs.");
    }

    ++bodyCount;
    bodyCounter.add();
    return {this, key, *id};
}

void World::forgetBody(Simulation::BodyId id) {
    forgetMotion(id);
    destroyedBodies.push_back(id);
    --bodyCount;
    bodyCounter.remove();
}

void World::forgetMotion(Simulation::BodyId id) {
    if (motions.empty()) {
        return;
    }

    motions[simulation->getBodySlot(id)].id = 0;
}

void World::takeDestroyedBodies(std::vector<std::uint32_t>& ids) {
    ids.insert(ids.end(), destroyedBodies.begin(), destroyedBodies.end());
    destroyedBodies.clear();
}

void World::checkLive() const {
    static_cast<void>(getSimulation());
}

void World::step(float deltaSeconds) {
    checkLive();
    if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0F) {
        throw std::invalid_argument("A step of a 3D physics world takes a time of zero seconds or more.");
    }

    runHooks(StepPhase::Before, deltaSeconds);
    if (simulation == nullptr) {
        return;
    }

    const Simulation::StepReport report = simulation->step(deltaSeconds);
    lastStep = deltaSeconds;
    reportLimits(report);
    if (settings.interpolate) {
        recordMotion();
    }

    runHooks(StepPhase::After, deltaSeconds);
}

std::uint64_t World::addStepHook(StepPhase phase, StepHook hook) {
    checkLive();
    const std::uint64_t id = nextHook++;
    hooks.push_back(std::make_shared<Hook>(Hook{.id = id, .phase = phase, .function = std::move(hook)}));
    return id;
}

void World::removeStepHook(std::uint64_t id) {
    const auto found = std::ranges::find_if(hooks, [id](const std::shared_ptr<Hook>& hook) { return hook->id == id; });
    if (found == hooks.end()) {
        return;
    }

    (*found)->removed = true;
    hooks.erase(found);
}

// A hook may add, remove or run hooks and destroy the world, so the phase runs a snapshot of them, holding each one while it runs, and stops once the world is gone.
void World::runHooks(StepPhase phase, float deltaSeconds) {
    const std::size_t base = runningHooks.size();
    const RunningScope scope(runningHooks, base);
    runningHooks.insert(runningHooks.end(), hooks.begin(), hooks.end());
    const std::size_t end = runningHooks.size();

    for (std::size_t index = base; index < end && simulation != nullptr; ++index) {
        const std::shared_ptr<Hook> hook = runningHooks[index];
        if (!hook->removed && hook->phase == phase) {
            hook->function(deltaSeconds);
        }
    }
}

// A body that moved in the step before keeps its transform of that step as the previous one, and a body seen moving for the first time starts from where it is.
void World::recordMotion() {
    ++motionStep;
    simulation->collectActiveBodies(activeBodies);
    for (const Simulation::BodyId id : activeBodies) {
        Motion& motion = motions[simulation->getBodySlot(id)];
        const Body::Transform current = simulation->getBodyTransform(id);
        const bool continues = motion.id == id && motion.step + 1 == motionStep;
        motion.previous = continues ? motion.current : current;
        motion.current = current;
        motion.id = id;
        motion.step = motionStep;
    }
}

Body::Transform World::blendMotion(Simulation::BodyId id, float blend) const {
    const Motion& motion = motions[simulation->getBodySlot(id)];
    if (motion.id != id || motion.step != motionStep) {
        return simulation->getBodyTransform(id);
    }

    return {.position = math::Vec3::lerp(motion.previous.position, motion.current.position, blend), .rotation = math::Quat::slerp(motion.previous.rotation, motion.current.rotation, blend)};
}

Body::Transform World::getInterpolatedTransform(const Body& body, float blend) const {
    if (!settings.interpolate) {
        throw std::logic_error("Only a 3D physics world made with \"interpolate\" blends the transforms of its bodies.");
    }

    if (body.getWorldKey() != key || !body.isValid()) {
        throw std::invalid_argument("Body transforms need live bodies of this world.");
    }

    return blendMotion(body.getId(), std::clamp(blend, 0.0F, 1.0F));
}

void World::checkTransforms(std::span<const Body> bodies, std::size_t valueCount) const {
    checkLive();
    if (valueCount < bodies.size() * kTransformFloats) {
        throw std::invalid_argument("Body transforms take seven floats for each body.");
    }

    for (const Body& body : bodies) {
        if (body.getWorldKey() != key || !simulation->isBodyValid(body.getId())) {
            throw std::invalid_argument("Body transforms need live bodies of this world.");
        }
    }
}

void World::collectIds(std::span<const Body> bodies) const {
    transformIds.clear();
    for (const Body& body : bodies) {
        transformIds.push_back(body.getId());
    }
}

void World::readTransforms(std::span<const Body> bodies, std::span<float> values, std::optional<float> blend) const {
    checkTransforms(bodies, values.size());
    if (blend && !settings.interpolate) {
        throw std::logic_error("Only a 3D physics world made with \"interpolate\" blends the transforms of its bodies.");
    }

    if (!blend) {
        collectIds(bodies);
        simulation->readBodyTransforms(transformIds, values.first(bodies.size() * kTransformFloats));
        return;
    }

    const float amount = std::clamp(*blend, 0.0F, 1.0F);
    for (std::size_t index = 0; index < bodies.size(); ++index) {
        const Body::Transform transform = blendMotion(bodies[index].getId(), amount);
        const std::span<float> target = values.subspan(index * kTransformFloats, kTransformFloats);
        target[0] = transform.position.x;
        target[1] = transform.position.y;
        target[2] = transform.position.z;
        target[3] = transform.rotation.x;
        target[4] = transform.rotation.y;
        target[5] = transform.rotation.z;
        target[6] = transform.rotation.w;
    }
}

// The rotations are normalized here, so a buffer of rotations that drifted from a length of one still places the bodies.
void World::writeTransforms(std::span<const Body> bodies, std::span<const float> values) {
    checkTransforms(bodies, values.size());
    for (std::size_t index = 0; index < bodies.size(); ++index) {
        const std::span<const float> source = values.subspan(index * kTransformFloats, kTransformFloats);
        const math::Vec3 position{source[0], source[1], source[2]};
        const math::Quat rotation{source[3], source[4], source[5], source[6]};
        if (!position.isFinite() || !std::isfinite(rotation.getLength()) || rotation.getLength() <= 0.0F) {
            throw std::invalid_argument("Body transforms take finite positions and rotations that are not zero.");
        }
    }

    collectIds(bodies);
    simulation->writeBodyTransforms(transformIds, values.first(bodies.size() * kTransformFloats));
    for (const Simulation::BodyId id : transformIds) {
        forgetMotion(id);
    }
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
