#pragma once

#include <DetourLocalBoundary.h>
#include <DetourNavMeshQuery.h>
#include <DetourObstacleAvoidance.h>
#include <DetourPathCorridor.h>
#include <DetourPathQueue.h>
#include <DetourProximityGrid.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include "3d/navigation/AreaFilter.hpp"
#include "3d/navigation/NavMeshState.hpp"
#include "haylen/3d/navigation/Crowd.hpp"

namespace haylen::navigation3d {

// What a crowd owns: its agents in fixed slots, the filters they name, the proximity grid that finds neighbors, the path queue that plans long paths within the budget of a step, and one query of the mesh and of avoidance for each thread that steps agents.
struct Crowd::State {
    static constexpr std::size_t kMaxNeighbors = 6;
    static constexpr std::size_t kMaxCorners = 4;
    static constexpr std::size_t kLocalNodes = 512;
    static constexpr std::size_t kQueueNodes = 4096;
    static constexpr std::size_t kQueuedAgents = 8;
    static constexpr std::size_t kQuickIterations = 20;
    static constexpr std::size_t kQuickPolygons = 32;
    static constexpr std::size_t kGrainSize = 32;
    static constexpr int kLookAhead = 10;
    static constexpr float kReplanDelay = 1.0F;
    static constexpr float kTopologyInterval = 0.5F;
    static constexpr float kCollisionResolve = 0.7F;
    static constexpr int kCollisionIterations = 4;

    // How far the request of a target gets at once before the rest of its path goes to the queue.
    enum class Target : std::uint8_t {
        None,
        Requesting,
        WaitingForQueue,
        WaitingForPath,
        Valid,
        Failed,
        Velocity,
    };

    struct Neighbor {
        std::size_t slot = 0;
        float distance = 0.0F;
    };

    // The crossing of a link: the agent first walks to its start and then crosses it, and a manual crossing waits at the start.
    struct Crossing {
        bool active = false;
        bool waiting = false;
        std::array<float, 3> from{};
        std::array<float, 3> start{};
        std::array<float, 3> end{};
        dtPolyRef polygon = 0;
        float time = 0.0F;
        float approach = 0.0F;
        float duration = 0.0F;
        Link::Kind kind = Link::Kind::Jump;
    };

    struct Agent {
        bool active = false;
        std::uint16_t generation = 0;
        AgentOptions options;
        AgentState state = AgentState::Invalid;
        Target target = Target::None;
        dtPathCorridor corridor;
        dtLocalBoundary boundary;
        std::array<float, 3> position{};
        std::array<float, 3> velocity{};
        std::array<float, 3> desired{};
        std::array<float, 3> planned{};
        std::array<float, 3> displacement{};
        float desiredSpeed = 0.0F;
        std::array<Neighbor, kMaxNeighbors> neighbors{};
        std::size_t neighborCount = 0;
        std::array<float, kMaxCorners * 3> corners{};
        std::array<unsigned char, kMaxCorners> cornerFlags{};
        std::array<dtPolyRef, kMaxCorners> cornerPolygons{};
        std::size_t cornerCount = 0;
        dtPolyRef targetPolygon = 0;
        std::array<float, 3> targetPosition{};
        dtPathQueueRef pathRequest = DT_PATHQ_INVALID;
        bool replan = false;
        bool partial = false;
        float replanTime = 0.0F;
        float topologyTime = 0.0F;
        Crossing crossing;
    };

    // The queries that one thread uses while it steps a share of the agents.
    struct Workspace {
        std::unique_ptr<dtNavMeshQuery, decltype(&dtFreeNavMeshQuery)> query{nullptr, &dtFreeNavMeshQuery};
        std::unique_ptr<dtObstacleAvoidanceQuery, decltype(&dtFreeObstacleAvoidanceQuery)> avoidance{nullptr, &dtFreeObstacleAvoidanceQuery};
    };

    using Phase = void (State::*)(Agent& agent, Workspace& workspace, float deltaSeconds);

    std::shared_ptr<NavMesh::State> mesh;
    Options options;
    std::array<float, 3> placementExtents{};
    std::unique_ptr<Agent[]> agents;
    std::vector<AgentId> ids;
    std::vector<std::size_t> active;
    std::vector<AreaFilter> filters;
    std::unique_ptr<dtProximityGrid, decltype(&dtFreeProximityGrid)> grid{nullptr, &dtFreeProximityGrid};
    dtPathQueue pathQueue;
    std::vector<dtPolyRef> pathResult;
    std::vector<Workspace> workspaces;
    std::vector<std::size_t> freeWorkspaces;
    std::mutex workspaceMutex;
    std::vector<LinkEntry> linkEntries;
    std::array<dtObstacleAvoidanceParams, 4> avoidance{};

    State(const NavMesh& navMesh, const Options& crowdOptions);

    [[nodiscard]] static AgentId makeId(std::size_t slot, std::uint16_t generation) noexcept {
        return (static_cast<AgentId>(generation) << 16U) | static_cast<AgentId>(slot);
    }
    [[nodiscard]] std::optional<std::size_t> findSlot(AgentId id) const noexcept;
    [[nodiscard]] std::size_t requireSlot(AgentId id) const;
    [[nodiscard]] Workspace& getMainWorkspace() noexcept {
        return workspaces.front();
    }
    [[nodiscard]] const AreaFilter& getFilter(const Agent& agent) const noexcept {
        return filters[agent.options.filter];
    }

    // Gives the crowd a workspace for each thread that may step agents at once.
    void addWorkspaces(std::size_t count);
    void validate(const AgentOptions& agentOptions) const;
    void rebuildIds();
    void requestTarget(Agent& agent, dtPolyRef polygon, const std::array<float, 3>& position, bool replan);

    // Runs a phase for every active agent, spread over the job system with a workspace for each share.
    void forEachAgent(core::JobSystem* jobs, Phase phase, float deltaSeconds);
    void runShare(std::size_t begin, std::size_t end, Phase phase, float deltaSeconds);

    void checkPath(Agent& agent, Workspace& workspace, float deltaSeconds);
    void updateRequests();
    void optimizeTopology(float deltaSeconds);
    void findNeighbors(Agent& agent, Workspace& workspace, float deltaSeconds);
    void findCorners(Agent& agent, Workspace& workspace, float deltaSeconds);
    void enterLinks();
    void steer(Agent& agent, Workspace& workspace, float deltaSeconds);
    void plan(Agent& agent, Workspace& workspace, float deltaSeconds);
    void integrate(Agent& agent, Workspace& workspace, float deltaSeconds);
    void separate(Agent& agent, Workspace& workspace, float deltaSeconds);
    void displace(Agent& agent, Workspace& workspace, float deltaSeconds);
    void constrain(Agent& agent, Workspace& workspace, float deltaSeconds);
    void crossLinks(float deltaSeconds);
};

} // namespace haylen::navigation3d
