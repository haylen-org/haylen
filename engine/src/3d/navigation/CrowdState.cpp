#include "3d/navigation/CrowdState.hpp"

#include <DetourCommon.h>
#include <DetourNavMesh.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

#include "3d/navigation/DetourVector.hpp"
#include "haylen/core/JobSystem.hpp"

namespace haylen::navigation3d {

Crowd::State::State(const NavMesh& navMesh, const Options& crowdOptions) : mesh(navMesh.state), options(crowdOptions) {
    if (options.maxAgents == 0 || options.maxAgents > kMaxAgents) {
        throw std::invalid_argument("A crowd holds from 1 to 65535 agents.");
    }
    if (!std::isfinite(options.maxAgentRadius) || options.maxAgentRadius <= 0.0F) {
        throw std::invalid_argument("The largest radius of the agents of a crowd must be a finite number of meters above zero.");
    }
    if (options.pathBudget == 0) {
        throw std::invalid_argument("The path budget of a crowd must be at least 1.");
    }

    // A placement looks farther than the radius, since it also brings back agents that left the mesh.
    placementExtents = {options.maxAgentRadius * 2.0F, options.maxAgentRadius * 1.5F, options.maxAgentRadius * 2.0F};
    grid.reset(dtAllocProximityGrid());
    if (grid == nullptr || !grid->init(static_cast<int>(options.maxAgents * 4), options.maxAgentRadius * 3.0F)) {
        throw std::runtime_error("A crowd did not start, since the system ran out of memory.");
    }
    if (!pathQueue.init(static_cast<int>(kMaxCorridor), static_cast<int>(kQueueNodes), mesh->mesh.get())) {
        throw std::runtime_error("A crowd did not start, since the system ran out of memory.");
    }

    pathResult.resize(kMaxCorridor);
    agents = std::make_unique<Agent[]>(options.maxAgents);
    for (std::size_t slot = 0; slot < options.maxAgents; ++slot) {
        if (!agents[slot].corridor.init(static_cast<int>(kMaxCorridor))) {
            throw std::runtime_error("A crowd did not start, since the system ran out of memory.");
        }
    }

    ids.reserve(options.maxAgents);
    active.reserve(options.maxAgents);
    linkEntries.reserve(options.maxAgents);
    filters.reserve(kMaxFilters);
    filters.emplace_back(QueryFilter{});
    addWorkspaces(1);

    // The levels of avoidance sample more directions, rings and refinements of the best velocity.
    constexpr std::array<std::array<unsigned char, 3>, 4> kSampling{{{0, 0, 0}, {5, 2, 1}, {5, 2, 2}, {7, 2, 3}}};
    for (std::size_t level = 0; level < avoidance.size(); ++level) {
        dtObstacleAvoidanceParams& params = avoidance[level];
        params.velBias = 0.4F;
        params.weightDesVel = 2.0F;
        params.weightCurVel = 0.75F;
        params.weightSide = 0.75F;
        params.weightToi = 2.5F;
        params.horizTime = 2.5F;
        params.gridSize = 33;
        params.adaptiveDivs = kSampling[level][0];
        params.adaptiveRings = kSampling[level][1];
        params.adaptiveDepth = kSampling[level][2];
    }
}

void Crowd::State::addWorkspaces(std::size_t count) {
    const std::scoped_lock lock(workspaceMutex);
    while (workspaces.size() < count) {
        Workspace workspace;
        workspace.query.reset(dtAllocNavMeshQuery());
        workspace.avoidance.reset(dtAllocObstacleAvoidanceQuery());
        if (workspace.query == nullptr || workspace.avoidance == nullptr || dtStatusFailed(workspace.query->init(mesh->mesh.get(), static_cast<int>(kLocalNodes))) || !workspace.avoidance->init(static_cast<int>(kMaxNeighbors), 8)) {
            throw std::runtime_error("A crowd did not start, since the system ran out of memory.");
        }

        freeWorkspaces.push_back(workspaces.size());
        workspaces.push_back(std::move(workspace));
    }
}

std::optional<std::size_t> Crowd::State::findSlot(AgentId id) const noexcept {
    const std::size_t slot = id & 0xFFFFU;
    if (slot >= options.maxAgents || !agents[slot].active || agents[slot].generation != (id >> 16U)) {
        return std::nullopt;
    }

    return slot;
}

std::size_t Crowd::State::requireSlot(AgentId id) const {
    const std::optional<std::size_t> slot = findSlot(id);
    if (!slot) {
        throw std::invalid_argument("The crowd has no agent " + std::to_string(id) + ".");
    }

    return *slot;
}

void Crowd::State::validate(const AgentOptions& agentOptions) const {
    const std::array<float, 7> positive{agentOptions.radius, agentOptions.height, agentOptions.maxSpeed, agentOptions.maxAcceleration, agentOptions.climbSpeed, agentOptions.separationWeight + 1.0F, agentOptions.jumpHeight + 1.0F};
    const bool finite = agentOptions.position.isFinite() && std::isfinite(agentOptions.collisionRange) && std::isfinite(agentOptions.pathOptimizationRange) && agentOptions.collisionRange >= 0.0F && agentOptions.pathOptimizationRange >= 0.0F;
    const bool sized = std::ranges::all_of(positive, [](float value) { return std::isfinite(value) && value > 0.0F; });
    if (!finite || !sized || agentOptions.separationWeight < 0.0F || agentOptions.jumpHeight < 0.0F) {
        throw std::invalid_argument("The radius, height, speeds and acceleration of an agent must be finite numbers above zero, and its ranges, separation weight and jump height finite numbers of at least zero.");
    }
    if (agentOptions.radius > options.maxAgentRadius) {
        throw std::invalid_argument("The radius of an agent cannot pass the largest radius of the agents of its crowd.");
    }
    if (agentOptions.filter >= filters.size()) {
        throw std::invalid_argument("The crowd has no filter " + std::to_string(agentOptions.filter) + ".");
    }
}

void Crowd::State::rebuildIds() {
    ids.clear();
    for (std::size_t slot = 0; slot < options.maxAgents; ++slot) {
        if (agents[slot].active) {
            ids.push_back(makeId(slot, agents[slot].generation));
        }
    }
}

void Crowd::State::requestTarget(Agent& agent, dtPolyRef polygon, const std::array<float, 3>& position, bool replan) {
    agent.targetPolygon = polygon;
    agent.targetPosition = position;
    agent.pathRequest = DT_PATHQ_INVALID;
    agent.replan = replan;
    agent.target = polygon != 0 ? Target::Requesting : Target::Failed;
}

void Crowd::State::runShare(std::size_t begin, std::size_t end, Phase phase, float deltaSeconds) {
    std::size_t index = 0;
    {
        const std::scoped_lock lock(workspaceMutex);
        index = freeWorkspaces.back();
        freeWorkspaces.pop_back();
    }

    Workspace& workspace = workspaces[index];
    for (std::size_t item = begin; item < end; ++item) {
        (this->*phase)(agents[active[item]], workspace, deltaSeconds);
    }

    const std::scoped_lock lock(workspaceMutex);
    freeWorkspaces.push_back(index);
}

void Crowd::State::forEachAgent(core::JobSystem* jobs, Phase phase, float deltaSeconds) {
    if (jobs == nullptr) {
        runShare(0, active.size(), phase, deltaSeconds);
        return;
    }

    // clang-format off
    jobs->parallelFor(0, active.size(), kGrainSize, [this, phase, deltaSeconds](std::size_t begin, std::size_t end) {
        runShare(begin, end, phase, deltaSeconds);
    });
    // clang-format on
}

void Crowd::State::checkPath(Agent& agent, Workspace& workspace, float deltaSeconds) {
    if (agent.state != AgentState::Walking) {
        return;
    }

    agent.replanTime += deltaSeconds;
    const AreaFilter& filter = getFilter(agent);
    dtNavMeshQuery& query = *workspace.query;
    bool replan = false;

    // An agent whose polygon went away, such as with a tile that rebuilt, steps back onto the mesh near it.
    dtPolyRef polygon = agent.corridor.getFirstPoly();
    if (!query.isValidPolyRef(polygon, &filter)) {
        std::array<float, 3> nearest = agent.position;
        polygon = 0;
        query.findNearestPoly(agent.position.data(), placementExtents.data(), &filter, &polygon, nearest.data());
        if (polygon == 0) {
            agent.corridor.reset(0, nearest.data());
            agent.partial = false;
            agent.boundary.reset();
            agent.state = AgentState::Invalid;
            return;
        }

        agent.corridor.fixPathStart(polygon, nearest.data());
        agent.boundary.reset();
        agent.position = nearest;
        replan = true;
    }

    if (agent.target == Target::None || agent.target == Target::Velocity) {
        return;
    }

    // A target whose polygon went away moves to the mesh near it, or the request ends.
    if (agent.target != Target::Failed && !query.isValidPolyRef(agent.targetPolygon, &filter)) {
        std::array<float, 3> nearest = agent.targetPosition;
        agent.targetPolygon = 0;
        query.findNearestPoly(agent.targetPosition.data(), placementExtents.data(), &filter, &agent.targetPolygon, nearest.data());
        agent.targetPosition = nearest;
        replan = true;
        if (agent.targetPolygon == 0) {
            agent.corridor.reset(polygon, agent.position.data());
            agent.partial = false;
            agent.target = Target::None;
            return;
        }
    }

    if (!agent.corridor.isValid(kLookAhead, &query, &filter)) {
        replan = true;
    }
    if (agent.target == Target::Valid && agent.replanTime > kReplanDelay && agent.corridor.getPathCount() < kLookAhead && agent.corridor.getLastPoly() != agent.targetPolygon) {
        replan = true;
    }
    if (replan) {
        requestTarget(agent, agent.targetPolygon, agent.targetPosition, true);
    }
}

void Crowd::State::updateRequests() {
    Workspace& workspace = getMainWorkspace();
    dtNavMeshQuery& query = *workspace.query;
    std::array<std::size_t, kQueuedAgents> queue{};
    std::size_t queued = 0;

    // A new target first gets a quick search toward it, and a path that this does not reach waits in the queue, which takes the agents that waited longest first.
    for (const std::size_t slot : active) {
        Agent& agent = agents[slot];
        if (agent.state == AgentState::Invalid || agent.target == Target::None || agent.target == Target::Velocity) {
            continue;
        }

        if (agent.target == Target::Requesting) {
            const AreaFilter& filter = getFilter(agent);
            const dtPolyRef* corridor = agent.corridor.getPath();
            const int corridorCount = agent.corridor.getPathCount();
            std::array<dtPolyRef, kQuickPolygons> found{};
            int foundCount = 0;
            std::array<float, 3> reach = agent.targetPosition;

            query.initSlicedFindPath(corridor[0], agent.targetPolygon, agent.position.data(), agent.targetPosition.data(), &filter);
            query.updateSlicedFindPath(static_cast<int>(kQuickIterations), nullptr);
            const dtStatus status = agent.replan ? query.finalizeSlicedFindPathPartial(corridor, corridorCount, found.data(), &foundCount, static_cast<int>(kQuickPolygons)) : query.finalizeSlicedFindPath(found.data(), &foundCount, static_cast<int>(kQuickPolygons));
            if (dtStatusFailed(status) || foundCount == 0) {
                foundCount = 0;
            } else if (found[static_cast<std::size_t>(foundCount) - 1] != agent.targetPolygon && dtStatusFailed(query.closestPointOnPoly(found[static_cast<std::size_t>(foundCount) - 1], agent.targetPosition.data(), reach.data(), nullptr))) {
                foundCount = 0;
            }

            if (foundCount == 0) {
                reach = agent.position;
                found[0] = corridor[0];
                foundCount = 1;
            }

            agent.corridor.setCorridor(reach.data(), found.data(), foundCount);
            agent.boundary.reset();
            agent.partial = false;
            if (found[static_cast<std::size_t>(foundCount) - 1] == agent.targetPolygon) {
                agent.target = Target::Valid;
                agent.replanTime = 0.0F;
            } else {
                agent.target = Target::WaitingForQueue;
            }
        }

        if (agent.target != Target::WaitingForQueue) {
            continue;
        }

        // The queue keeps the agents that waited longest, in that order.
        std::size_t place = queued;
        while (place > 0 && agents[queue[place - 1]].replanTime < agent.replanTime) {
            --place;
        }
        if (place >= kQueuedAgents) {
            continue;
        }

        const std::size_t moved = std::min(queued, kQueuedAgents - 1) - place;
        std::copy_backward(queue.begin() + static_cast<std::ptrdiff_t>(place), queue.begin() + static_cast<std::ptrdiff_t>(place + moved), queue.begin() + static_cast<std::ptrdiff_t>(place + moved + 1));
        queue[place] = slot;
        queued = std::min(queued + 1, kQueuedAgents);
    }

    for (std::size_t index = 0; index < queued; ++index) {
        Agent& agent = agents[queue[index]];
        agent.pathRequest = pathQueue.request(agent.corridor.getLastPoly(), agent.targetPolygon, agent.corridor.getTarget(), agent.targetPosition.data(), &getFilter(agent));
        if (agent.pathRequest != DT_PATHQ_INVALID) {
            agent.target = Target::WaitingForPath;
        }
    }

    pathQueue.update(static_cast<int>(options.pathBudget));

    for (const std::size_t slot : active) {
        Agent& agent = agents[slot];
        if (agent.target != Target::WaitingForPath) {
            continue;
        }

        dtStatus status = pathQueue.getRequestStatus(agent.pathRequest);
        if (dtStatusFailed(status)) {
            agent.pathRequest = DT_PATHQ_INVALID;
            agent.target = agent.targetPolygon != 0 ? Target::Requesting : Target::Failed;
            agent.replanTime = 0.0F;
            continue;
        }
        if (!dtStatusSucceed(status)) {
            continue;
        }

        // The agent moved while its path was planned, so the old corridor up to where the request started goes in front of the new path.
        const dtPolyRef* corridor = agent.corridor.getPath();
        const int corridorCount = agent.corridor.getPathCount();
        std::array<float, 3> reach = agent.targetPosition;
        int resultCount = 0;
        status = pathQueue.getPathResult(agent.pathRequest, pathResult.data(), &resultCount, static_cast<int>(kMaxCorridor));
        bool valid = !dtStatusFailed(status) && resultCount > 0 && corridor[corridorCount - 1] == pathResult[0];
        agent.partial = dtStatusDetail(status, DT_PARTIAL_RESULT);
        if (valid && corridorCount > 1) {
            resultCount = std::min(resultCount, static_cast<int>(kMaxCorridor) - (corridorCount - 1));
            std::copy_backward(pathResult.begin(), pathResult.begin() + resultCount, pathResult.begin() + resultCount + corridorCount - 1);
            std::copy(corridor, corridor + corridorCount - 1, pathResult.begin());
            resultCount += corridorCount - 1;

            // A polygon that the path enters and leaves again at once is a step back, which the path drops.
            for (int index = 1; index + 1 < resultCount; ++index) {
                if (pathResult[static_cast<std::size_t>(index) - 1] == pathResult[static_cast<std::size_t>(index) + 1]) {
                    std::copy(pathResult.begin() + index + 1, pathResult.begin() + resultCount, pathResult.begin() + index - 1);
                    resultCount -= 2;
                    index = std::max(0, index - 2);
                }
            }
        }
        if (valid && pathResult[static_cast<std::size_t>(resultCount) - 1] != agent.targetPolygon) {
            std::array<float, 3> nearest{};
            valid = dtStatusSucceed(getMainWorkspace().query->closestPointOnPoly(pathResult[static_cast<std::size_t>(resultCount) - 1], reach.data(), nearest.data(), nullptr));
            reach = nearest;
            agent.partial = true;
        }

        if (valid) {
            agent.corridor.setCorridor(reach.data(), pathResult.data(), resultCount);
            agent.boundary.reset();
            agent.target = Target::Valid;
        } else {
            agent.target = Target::Failed;
        }
        agent.replanTime = 0.0F;
    }
}

void Crowd::State::optimizeTopology(float deltaSeconds) {
    // Once a while, the agent that waited longest looks for a shorter corridor among the polygons it already knows.
    Agent* chosen = nullptr;
    for (const std::size_t slot : active) {
        Agent& agent = agents[slot];
        if (agent.state != AgentState::Walking || agent.target == Target::None || agent.target == Target::Velocity || !agent.options.optimizeTopology) {
            continue;
        }

        agent.topologyTime += deltaSeconds;
        if (agent.topologyTime >= kTopologyInterval && (chosen == nullptr || agent.topologyTime > chosen->topologyTime)) {
            chosen = &agent;
        }
    }

    if (chosen != nullptr) {
        chosen->corridor.optimizePathTopology(getMainWorkspace().query.get(), &getFilter(*chosen));
        chosen->topologyTime = 0.0F;
    }
}

void Crowd::State::findNeighbors(Agent& agent, Workspace& workspace, float) {
    if (agent.state != AgentState::Walking) {
        return;
    }

    const AreaFilter& filter = getFilter(agent);
    const float range = agent.options.collisionRange;
    const float refresh = range * 0.25F;
    if (dtVdist2DSqr(agent.position.data(), agent.boundary.getCenter()) > refresh * refresh || !agent.boundary.isValid(workspace.query.get(), &filter)) {
        agent.boundary.update(agent.corridor.getFirstPoly(), agent.position.data(), range, workspace.query.get(), &filter);
    }

    // The nearest agents within the range that overlap the agent in height, closest first.
    constexpr int kCandidates = 32;
    std::array<unsigned short, kCandidates> candidates{};
    const int count = grid->queryItems(agent.position[0] - range, agent.position[2] - range, agent.position[0] + range, agent.position[2] + range, candidates.data(), kCandidates);
    agent.neighborCount = 0;
    for (int index = 0; index < count; ++index) {
        const std::size_t slot = active[candidates[static_cast<std::size_t>(index)]];
        const Agent& other = agents[slot];
        if (&other == &agent || std::fabs(agent.position[1] - other.position[1]) >= (agent.options.height + other.options.height) * 0.5F) {
            continue;
        }

        const float distance = dtVdist2DSqr(agent.position.data(), other.position.data());
        if (distance > range * range) {
            continue;
        }

        std::size_t place = agent.neighborCount;
        while (place > 0 && (agent.neighbors[place - 1].distance > distance || (agent.neighbors[place - 1].distance == distance && agent.neighbors[place - 1].slot > slot))) {
            --place;
        }
        if (place >= kMaxNeighbors) {
            continue;
        }

        const std::size_t moved = std::min(agent.neighborCount, kMaxNeighbors - 1) - place;
        std::copy_backward(agent.neighbors.begin() + static_cast<std::ptrdiff_t>(place), agent.neighbors.begin() + static_cast<std::ptrdiff_t>(place + moved), agent.neighbors.begin() + static_cast<std::ptrdiff_t>(place + moved + 1));
        agent.neighbors[place] = {.slot = slot, .distance = distance};
        agent.neighborCount = std::min(agent.neighborCount + 1, kMaxNeighbors);
    }
}

void Crowd::State::findCorners(Agent& agent, Workspace& workspace, float) {
    if (agent.state != AgentState::Walking || agent.target == Target::None || agent.target == Target::Velocity) {
        return;
    }

    const AreaFilter& filter = getFilter(agent);
    agent.cornerCount = static_cast<std::size_t>(agent.corridor.findCorners(agent.corners.data(), agent.cornerFlags.data(), agent.cornerPolygons.data(), static_cast<int>(kMaxCorners), workspace.query.get(), &filter));

    // A corner after the next one that the agent already sees shortens the corridor to it.
    if (agent.options.optimizeVisibility && agent.cornerCount > 0) {
        const float* next = &agent.corners[std::min<std::size_t>(1, agent.cornerCount - 1) * 3];
        agent.corridor.optimizePathVisibility(next, agent.options.pathOptimizationRange, workspace.query.get(), &filter);
    }
}

void Crowd::State::enterLinks() {
    linkEntries.clear();
    dtNavMeshQuery& query = *getMainWorkspace().query;
    for (const std::size_t slot : active) {
        Agent& agent = agents[slot];
        if (agent.state != AgentState::Walking || agent.target == Target::None || agent.target == Target::Velocity || agent.cornerCount == 0) {
            continue;
        }

        // An agent near a corner that starts a link takes the link.
        const std::size_t last = agent.cornerCount - 1;
        const float trigger = agent.options.radius * 2.25F;
        if ((agent.cornerFlags[last] & DT_STRAIGHTPATH_OFFMESH_CONNECTION) == 0 || dtVdist2DSqr(agent.position.data(), &agent.corners[last * 3]) >= trigger * trigger) {
            continue;
        }

        Crossing& crossing = agent.crossing;
        std::array<dtPolyRef, 2> polygons{};
        const dtPolyRef link = agent.cornerPolygons[last];
        if (!agent.corridor.moveOverOffmeshConnection(link, polygons.data(), crossing.start.data(), crossing.end.data(), &query)) {
            continue;
        }

        const dtOffMeshConnection* connection = mesh->mesh->getOffMeshConnectionByRef(link);
        const std::uint32_t linkId = connection != nullptr ? connection->userId : 0;
        const auto found = mesh->links.find(linkId);
        crossing.kind = found != mesh->links.end() ? found->second.link.kind : Link::Kind::Jump;
        crossing.active = true;
        crossing.waiting = agent.options.linkTraversal == LinkTraversal::Manual;
        crossing.from = agent.position;
        crossing.polygon = polygons[1];
        crossing.time = 0.0F;
        crossing.approach = dtVdist(crossing.from.data(), crossing.start.data()) / agent.options.maxSpeed;

        const float length = dtVdist(crossing.start.data(), crossing.end.data());
        switch (crossing.kind) {
        case Link::Kind::Teleport:
            crossing.duration = 0.0F;
            break;
        case Link::Kind::Ladder:
            crossing.duration = length / agent.options.climbSpeed;
            break;
        case Link::Kind::Jump:
            crossing.duration = dtVdist2D(crossing.start.data(), crossing.end.data()) / agent.options.maxSpeed;
            break;
        }

        agent.state = AgentState::OnLink;
        agent.cornerCount = 0;
        agent.neighborCount = 0;
        linkEntries.push_back({.agent = makeId(slot, agent.generation), .link = linkId, .kind = crossing.kind, .start = DetourVector::toVec3(crossing.start.data()), .end = DetourVector::toVec3(crossing.end.data())});
    }
}

void Crowd::State::steer(Agent& agent, Workspace&, float) {
    if (agent.state != AgentState::Walking || agent.target == Target::None) {
        return;
    }

    std::array<float, 3> desired{};
    if (agent.target == Target::Velocity) {
        desired = agent.targetPosition;
        agent.desiredSpeed = dtVlen(agent.targetPosition.data());
    } else if (agent.cornerCount > 0) {
        // The agent heads for the next corner, or between it and the one after it to round the turn, and slows down within two radii of the end of its path.
        const float* first = agent.corners.data();
        dtVsub(desired.data(), first, agent.position.data());
        desired[1] = 0.0F;
        if (agent.options.anticipateTurns) {
            const float* second = &agent.corners[std::min<std::size_t>(1, agent.cornerCount - 1) * 3];
            std::array<float, 3> beyond{};
            dtVsub(beyond.data(), second, agent.position.data());
            beyond[1] = 0.0F;
            const float firstLength = dtVlen(desired.data());
            const float secondLength = dtVlen(beyond.data());
            if (secondLength > 0.001F) {
                dtVscale(beyond.data(), beyond.data(), 1.0F / secondLength);
            }
            desired[0] -= beyond[0] * firstLength * 0.5F;
            desired[2] -= beyond[2] * firstLength * 0.5F;
        }
        if (dtVlenSqr(desired.data()) > 0.0F) {
            dtVnormalize(desired.data());
        }

        const float slowDown = agent.options.radius * 2.0F;
        const bool end = (agent.cornerFlags[agent.cornerCount - 1] & DT_STRAIGHTPATH_END) != 0;
        const float distance = end ? std::min(dtVdist2D(agent.position.data(), &agent.corners[(agent.cornerCount - 1) * 3]), slowDown) : slowDown;
        agent.desiredSpeed = agent.options.maxSpeed;
        dtVscale(desired.data(), desired.data(), agent.desiredSpeed * distance / slowDown);
    }

    if (agent.options.separation && agent.neighborCount > 0) {
        const float range = agent.options.collisionRange;
        std::array<float, 3> push{};
        float weight = 0.0F;
        for (std::size_t index = 0; index < agent.neighborCount; ++index) {
            const Agent& other = agents[agent.neighbors[index].slot];
            std::array<float, 3> away{};
            dtVsub(away.data(), agent.position.data(), other.position.data());
            away[1] = 0.0F;
            const float squared = dtVlenSqr(away.data());
            if (squared < 0.00001F || squared > range * range) {
                continue;
            }

            const float distance = std::sqrt(squared);
            const float strength = agent.options.separationWeight * (1.0F - squared / (range * range));
            dtVmad(push.data(), push.data(), away.data(), strength / distance);
            weight += 1.0F;
        }

        if (weight > 0.0001F) {
            dtVmad(desired.data(), desired.data(), push.data(), 1.0F / weight);
            const float speed = dtVlenSqr(desired.data());
            const float limit = agent.desiredSpeed * agent.desiredSpeed;
            if (speed > limit) {
                dtVscale(desired.data(), desired.data(), limit / speed);
            }
        }
    }

    agent.desired = desired;
}

void Crowd::State::plan(Agent& agent, Workspace& workspace, float) {
    if (agent.state != AgentState::Walking) {
        return;
    }
    if (agent.options.avoidance == Avoidance::None) {
        agent.planned = agent.desired;
        return;
    }

    // The neighbors and the walls in front of the agent are what its velocity must not hit soon.
    dtObstacleAvoidanceQuery& query = *workspace.avoidance;
    query.reset();
    for (std::size_t index = 0; index < agent.neighborCount; ++index) {
        const Agent& other = agents[agent.neighbors[index].slot];
        query.addCircle(other.position.data(), other.options.radius, other.velocity.data(), other.desired.data());
    }
    for (int index = 0; index < agent.boundary.getSegmentCount(); ++index) {
        const float* segment = agent.boundary.getSegment(index);
        if (dtTriArea2D(agent.position.data(), segment, segment + 3) >= 0.0F) {
            query.addSegment(segment, segment + 3);
        }
    }

    query.sampleVelocityAdaptive(agent.position.data(), agent.options.radius, agent.desiredSpeed, agent.velocity.data(), agent.desired.data(), agent.planned.data(), &avoidance[static_cast<std::size_t>(agent.options.avoidance)]);
}

void Crowd::State::integrate(Agent& agent, Workspace&, float deltaSeconds) {
    if (agent.state != AgentState::Walking) {
        return;
    }

    // The velocity turns toward the planned one no faster than the acceleration allows.
    const float limit = agent.options.maxAcceleration * deltaSeconds;
    std::array<float, 3> change{};
    dtVsub(change.data(), agent.planned.data(), agent.velocity.data());
    const float size = dtVlen(change.data());
    if (size > limit) {
        dtVscale(change.data(), change.data(), limit / size);
    }

    dtVadd(agent.velocity.data(), agent.velocity.data(), change.data());
    if (dtVlen(agent.velocity.data()) > 0.0001F) {
        dtVmad(agent.position.data(), agent.position.data(), agent.velocity.data(), deltaSeconds);
    } else {
        agent.velocity = {};
    }
}

void Crowd::State::separate(Agent& agent, Workspace&, float) {
    agent.displacement = {};
    if (agent.state != AgentState::Walking) {
        return;
    }

    // Agents that overlap push each other apart, each one half of the way, and two on the same spot part to the sides of their way.
    float weight = 0.0F;
    const std::size_t self = static_cast<std::size_t>(&agent - agents.get());
    for (std::size_t index = 0; index < agent.neighborCount; ++index) {
        const std::size_t slot = agent.neighbors[index].slot;
        const Agent& other = agents[slot];
        std::array<float, 3> away{};
        dtVsub(away.data(), agent.position.data(), other.position.data());
        away[1] = 0.0F;

        const float reach = agent.options.radius + other.options.radius;
        float distance = dtVlenSqr(away.data());
        if (distance > reach * reach) {
            continue;
        }

        distance = std::sqrt(distance);
        float push = 0.0F;
        if (distance < 0.0001F) {
            away = self > slot ? std::array<float, 3>{-agent.desired[2], 0.0F, agent.desired[0]} : std::array<float, 3>{agent.desired[2], 0.0F, -agent.desired[0]};
            push = 0.01F;
        } else {
            push = (reach - distance) * 0.5F * kCollisionResolve / distance;
        }

        dtVmad(agent.displacement.data(), agent.displacement.data(), away.data(), push);
        weight += 1.0F;
    }

    if (weight > 0.0001F) {
        dtVscale(agent.displacement.data(), agent.displacement.data(), 1.0F / weight);
    }
}

void Crowd::State::displace(Agent& agent, Workspace&, float) {
    if (agent.state == AgentState::Walking) {
        dtVadd(agent.position.data(), agent.position.data(), agent.displacement.data());
    }
}

void Crowd::State::constrain(Agent& agent, Workspace& workspace, float) {
    if (agent.state != AgentState::Walking) {
        return;
    }

    // The agent slides along the mesh to where it moved, and an agent without a path keeps only the polygon it stands on.
    agent.corridor.movePosition(agent.position.data(), workspace.query.get(), &getFilter(agent));
    dtVcopy(agent.position.data(), agent.corridor.getPos());
    if (agent.target == Target::None || agent.target == Target::Velocity) {
        agent.corridor.reset(agent.corridor.getFirstPoly(), agent.position.data());
        agent.partial = false;
    }
}

void Crowd::State::crossLinks(float deltaSeconds) {
    for (const std::size_t slot : active) {
        Agent& agent = agents[slot];
        Crossing& crossing = agent.crossing;
        if (!crossing.active) {
            continue;
        }

        agent.velocity = {};
        agent.desired = {};
        crossing.time += deltaSeconds;

        // The agent first walks to the start of the link, where a manual crossing waits for the app.
        if (crossing.time < crossing.approach) {
            dtVlerp(agent.position.data(), crossing.from.data(), crossing.start.data(), crossing.time / crossing.approach);
            continue;
        }
        if (crossing.waiting) {
            agent.position = crossing.start;
            crossing.time = crossing.approach;
            continue;
        }

        const float progress = crossing.duration > 0.0F ? std::min(1.0F, (crossing.time - crossing.approach) / crossing.duration) : 1.0F;
        dtVlerp(agent.position.data(), crossing.start.data(), crossing.end.data(), progress);
        if (crossing.kind == Link::Kind::Jump) {
            agent.position[1] += 4.0F * agent.options.jumpHeight * progress * (1.0F - progress);
        }
        if (progress >= 1.0F) {
            crossing.active = false;
            agent.position = crossing.end;
            agent.state = AgentState::Walking;
        }
    }
}

} // namespace haylen::navigation3d
