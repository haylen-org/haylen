#include "haylen/3d/navigation/Crowd.hpp"

#include <DetourCommon.h>

#include <cmath>
#include <stdexcept>
#include <string>

#include "3d/navigation/CrowdState.hpp"
#include "3d/navigation/DetourVector.hpp"
#include "haylen/core/JobSystem.hpp"

namespace haylen::navigation3d {

const Crowd::Options Crowd::kDefaultOptions{};

Crowd::Crowd(const NavMesh& mesh, const Options& options) : state(std::make_unique<State>(mesh, options)) {}

Crowd::~Crowd() = default;

std::size_t Crowd::addFilter(const QueryFilter& filter) {
    if (state->filters.size() >= kMaxFilters) {
        throw std::invalid_argument("A crowd holds at most 16 filters.");
    }

    state->filters.emplace_back(filter);
    return state->filters.size() - 1;
}

void Crowd::setFilter(std::size_t index, const QueryFilter& filter) {
    if (index >= state->filters.size()) {
        throw std::invalid_argument("The crowd has no filter " + std::to_string(index) + ".");
    }

    state->filters[index] = AreaFilter(filter);
}

Crowd::AgentId Crowd::addAgent(const AgentOptions& options) {
    state->validate(options);

    std::size_t slot = 0;
    while (slot < state->options.maxAgents && state->agents[slot].active) {
        ++slot;
    }
    if (slot == state->options.maxAgents) {
        throw std::invalid_argument("The crowd is full with its " + std::to_string(state->options.maxAgents) + " agents, the \"maxAgents\" it was made with.");
    }

    State::Agent& agent = state->agents[slot];
    agent.options = options;
    if (agent.options.collisionRange == 0.0F) {
        agent.options.collisionRange = options.radius * 12.0F;
    }
    if (agent.options.pathOptimizationRange == 0.0F) {
        agent.options.pathOptimizationRange = options.radius * 30.0F;
    }

    const AreaFilter& filter = state->getFilter(agent);
    const std::array<float, 3> position = DetourVector::of(options.position);
    std::array<float, 3> nearest = position;
    dtPolyRef polygon = 0;
    if (dtStatusFailed(state->getMainWorkspace().query->findNearestPoly(position.data(), state->placementExtents.data(), &filter, &polygon, nearest.data()))) {
        nearest = position;
        polygon = 0;
    }

    agent.corridor.reset(polygon, nearest.data());
    agent.boundary.reset();
    agent.partial = false;
    agent.topologyTime = 0.0F;
    agent.replanTime = 0.0F;
    agent.neighborCount = 0;
    agent.cornerCount = 0;
    agent.desired = {};
    agent.planned = {};
    agent.velocity = {};
    agent.position = nearest;
    agent.desiredSpeed = 0.0F;
    agent.state = polygon != 0 ? AgentState::Walking : AgentState::Invalid;
    agent.target = State::Target::None;
    agent.crossing = {};
    agent.generation = static_cast<std::uint16_t>(agent.generation + 1);
    if (agent.generation == 0) {
        agent.generation = 1;
    }
    agent.active = true;

    state->rebuildIds();
    return State::makeId(slot, agent.generation);
}

bool Crowd::removeAgent(AgentId id) {
    const std::optional<std::size_t> slot = state->findSlot(id);
    if (!slot) {
        return false;
    }

    state->agents[*slot].active = false;
    state->rebuildIds();
    return true;
}

bool Crowd::hasAgent(AgentId id) const noexcept {
    return state->findSlot(id).has_value();
}

std::size_t Crowd::getAgentCount() const noexcept {
    return state->ids.size();
}

const std::vector<Crowd::AgentId>& Crowd::getAgents() const noexcept {
    return state->ids;
}

bool Crowd::setTarget(AgentId id, math::Vec3 target) {
    if (!target.isFinite()) {
        throw std::invalid_argument("The target of an agent must be a point of finite numbers.");
    }

    State::Agent& agent = state->agents[state->requireSlot(id)];
    const std::array<float, 3> point = DetourVector::of(target);
    std::array<float, 3> nearest = point;
    dtPolyRef polygon = 0;
    state->getMainWorkspace().query->findNearestPoly(point.data(), state->placementExtents.data(), &state->getFilter(agent), &polygon, nearest.data());
    if (polygon == 0) {
        return false;
    }

    state->requestTarget(agent, polygon, nearest, false);
    return true;
}

void Crowd::setTargets(std::span<const float> values) {
    if (values.size() < state->ids.size() * 3) {
        throw std::invalid_argument("The targets of " + std::to_string(state->ids.size()) + " agents need " + std::to_string(state->ids.size() * 3) + " numbers, but there are " + std::to_string(values.size()) + ".");
    }

    for (std::size_t index = 0; index < state->ids.size(); ++index) {
        setTarget(state->ids[index], {values[index * 3], values[index * 3 + 1], values[index * 3 + 2]});
    }
}

void Crowd::setVelocity(AgentId id, math::Vec3 velocity) {
    if (!velocity.isFinite()) {
        throw std::invalid_argument("The velocity of an agent must be a vector of finite numbers.");
    }

    State::Agent& agent = state->agents[state->requireSlot(id)];
    agent.targetPolygon = 0;
    agent.targetPosition = DetourVector::of(velocity);
    agent.pathRequest = DT_PATHQ_INVALID;
    agent.replan = false;
    agent.target = State::Target::Velocity;
}

void Crowd::clearTarget(AgentId id) {
    State::Agent& agent = state->agents[state->requireSlot(id)];
    agent.targetPolygon = 0;
    agent.targetPosition = {};
    agent.desired = {};
    agent.pathRequest = DT_PATHQ_INVALID;
    agent.replan = false;
    agent.target = State::Target::None;
}

bool Crowd::setPosition(AgentId id, math::Vec3 position) {
    if (!position.isFinite()) {
        throw std::invalid_argument("The position of an agent must be a point of finite numbers.");
    }

    State::Agent& agent = state->agents[state->requireSlot(id)];
    const std::array<float, 3> point = DetourVector::of(position);
    std::array<float, 3> nearest = point;
    dtPolyRef polygon = 0;
    state->getMainWorkspace().query->findNearestPoly(point.data(), state->placementExtents.data(), &state->getFilter(agent), &polygon, nearest.data());
    if (polygon == 0) {
        return false;
    }

    agent.corridor.reset(polygon, nearest.data());
    agent.boundary.reset();
    agent.position = nearest;
    agent.velocity = {};
    agent.crossing = {};
    agent.state = AgentState::Walking;
    if (agent.target != State::Target::None && agent.target != State::Target::Velocity && agent.targetPolygon != 0) {
        state->requestTarget(agent, agent.targetPolygon, agent.targetPosition, false);
    }
    return true;
}

bool Crowd::completeLink(AgentId id) {
    State::Agent& agent = state->agents[state->requireSlot(id)];
    if (!agent.crossing.active || !agent.crossing.waiting) {
        return false;
    }

    agent.crossing.waiting = false;
    agent.crossing.duration = 0.0F;
    return true;
}

void Crowd::step(float deltaSeconds, core::JobSystem* jobs) {
    if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0F) {
        throw std::invalid_argument("The step of a crowd takes a finite number of seconds of at least zero.");
    }
    if (jobs != nullptr) {
        state->addWorkspaces(jobs->getWorkerCount() + 1);
    }

    state->active.clear();
    for (std::size_t slot = 0; slot < state->options.maxAgents; ++slot) {
        if (state->agents[slot].active) {
            state->active.push_back(slot);
        }
    }

    state->forEachAgent(jobs, &State::checkPath, deltaSeconds);
    state->updateRequests();
    state->optimizeTopology(deltaSeconds);

    state->grid->clear();
    for (std::size_t index = 0; index < state->active.size(); ++index) {
        const State::Agent& agent = state->agents[state->active[index]];
        const float radius = agent.options.radius;
        state->grid->addItem(static_cast<unsigned short>(index), agent.position[0] - radius, agent.position[2] - radius, agent.position[0] + radius, agent.position[2] + radius);
    }

    state->forEachAgent(jobs, &State::findNeighbors, deltaSeconds);
    state->forEachAgent(jobs, &State::findCorners, deltaSeconds);
    state->enterLinks();
    state->forEachAgent(jobs, &State::steer, deltaSeconds);
    state->forEachAgent(jobs, &State::plan, deltaSeconds);
    state->forEachAgent(jobs, &State::integrate, deltaSeconds);
    for (int iteration = 0; iteration < State::kCollisionIterations; ++iteration) {
        state->forEachAgent(jobs, &State::separate, deltaSeconds);
        state->forEachAgent(jobs, &State::displace, deltaSeconds);
    }
    state->forEachAgent(jobs, &State::constrain, deltaSeconds);
    state->crossLinks(deltaSeconds);
}

math::Vec3 Crowd::getPosition(AgentId id) const {
    return DetourVector::toVec3(state->agents[state->requireSlot(id)].position.data());
}

math::Vec3 Crowd::getVelocity(AgentId id) const {
    return DetourVector::toVec3(state->agents[state->requireSlot(id)].velocity.data());
}

Crowd::AgentState Crowd::getState(AgentId id) const {
    return state->agents[state->requireSlot(id)].state;
}

bool Crowd::isPartial(AgentId id) const {
    const State::Agent& agent = state->agents[state->requireSlot(id)];
    return agent.target == State::Target::Failed || (agent.target != State::Target::None && agent.target != State::Target::Velocity && agent.partial);
}

std::optional<math::Vec3> Crowd::getTarget(AgentId id) const {
    const State::Agent& agent = state->agents[state->requireSlot(id)];
    if (agent.target == State::Target::None || agent.target == State::Target::Velocity) {
        return std::nullopt;
    }

    return DetourVector::toVec3(agent.targetPosition.data());
}

void Crowd::readPositions(std::span<float> values) const {
    if (values.size() < state->ids.size() * 3) {
        throw std::invalid_argument("The positions of " + std::to_string(state->ids.size()) + " agents need " + std::to_string(state->ids.size() * 3) + " numbers, but there is room for " + std::to_string(values.size()) + ".");
    }

    for (std::size_t index = 0; index < state->ids.size(); ++index) {
        const State::Agent& agent = state->agents[state->ids[index] & 0xFFFFU];
        std::copy(agent.position.begin(), agent.position.end(), values.begin() + static_cast<std::ptrdiff_t>(index * 3));
    }
}

void Crowd::readVelocities(std::span<float> values) const {
    if (values.size() < state->ids.size() * 3) {
        throw std::invalid_argument("The velocities of " + std::to_string(state->ids.size()) + " agents need " + std::to_string(state->ids.size() * 3) + " numbers, but there is room for " + std::to_string(values.size()) + ".");
    }

    for (std::size_t index = 0; index < state->ids.size(); ++index) {
        const State::Agent& agent = state->agents[state->ids[index] & 0xFFFFU];
        std::copy(agent.velocity.begin(), agent.velocity.end(), values.begin() + static_cast<std::ptrdiff_t>(index * 3));
    }
}

const std::vector<Crowd::LinkEntry>& Crowd::getLinkEntries() const noexcept {
    return state->linkEntries;
}

} // namespace haylen::navigation3d
