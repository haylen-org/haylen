#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/3d/navigation/Link.hpp"
#include "haylen/3d/navigation/NavMesh.hpp"
#include "haylen/3d/navigation/QueryFilter.hpp"
#include "haylen/core/EnumNames.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::core {
class JobSystem;
}

namespace haylen::navigation3d {

// Many agents that walk a navigation mesh together: each one follows a corridor of polygons toward its target, which path requests plan within a budget of each step, steers around the corners ahead, keeps clear of the walls near it and of the other agents on the plane of the mesh by sampling the velocities that do not hit them soon, and crosses the links on its way by their kind. Every agent and buffer is allocated when the crowd is made, so a step allocates no more as agents move. The work of each agent spreads over the job system, and a step gives the same result whatever the number of threads. Agents are named by ids that are never reused while the crowd lives.
class Crowd final {
  public:
    using AgentId = std::uint32_t;

    enum class AgentState : std::uint8_t {
        Walking,
        OnLink,
        Invalid,
    };

    // How carefully an agent picks a velocity that avoids the others: `None` walks straight at them, and each level above samples more velocities.
    enum class Avoidance : std::uint8_t {
        None,
        Low,
        Medium,
        High,
    };

    // How an agent crosses a link. `Automatic` crosses it by its kind: a teleport at once, a jump on an arc of `jumpHeight` at the speed of the agent and a ladder at `climbSpeed`. `Manual` stops the agent at the start of the link until the app calls `completeLink`, such as after an animation.
    enum class LinkTraversal : std::uint8_t {
        Automatic,
        Manual,
    };

    // The `maxAgents` that the crowd holds at once and the largest radius among them, and how many search steps the path requests of all agents take in each step at most.
    struct Options {
        std::size_t maxAgents = 256;
        float maxAgentRadius = 0.6F;
        std::size_t pathBudget = 100;
    };

    // An agent: where it starts, its size, its speed and acceleration in meters per second, how far it looks for walls and agents, how far ahead it shortens its corridor, and how it steers. A `collisionRange` or `pathOptimizationRange` of zero takes 12 and 30 times the radius. The `filter` is a filter that `addFilter` added, 0 for the default one.
    struct AgentOptions {
        math::Vec3 position;
        float radius = 0.6F;
        float height = 2.0F;
        float maxSpeed = 3.5F;
        float maxAcceleration = 8.0F;
        float collisionRange = 0.0F;
        float pathOptimizationRange = 0.0F;
        float separationWeight = 2.0F;
        Avoidance avoidance = Avoidance::High;
        bool anticipateTurns = true;
        bool optimizeVisibility = true;
        bool optimizeTopology = true;
        bool separation = false;
        std::size_t filter = 0;
        LinkTraversal linkTraversal = LinkTraversal::Automatic;
        float jumpHeight = 1.0F;
        float climbSpeed = 1.5F;
    };

    // An agent that reached a link in the last step, with the link and its ends.
    struct LinkEntry {
        AgentId agent = 0;
        std::uint32_t link = 0;
        Link::Kind kind = Link::Kind::Jump;
        math::Vec3 start;
        math::Vec3 end;
    };

    static constexpr std::size_t kMaxAgents = 65535;
    static constexpr std::size_t kMaxFilters = 16;
    static constexpr std::size_t kMaxCorridor = 256;

    // Throws `std::invalid_argument` for options out of their range.
    Crowd(const NavMesh& mesh, const Options& options = kDefaultOptions);
    ~Crowd();

    Crowd(const Crowd&) = delete;
    Crowd& operator=(const Crowd&) = delete;

    // Adds a filter that agents name by the index it returns, or replaces one. Throws `std::invalid_argument` past 16 filters or for an index that no filter has.
    std::size_t addFilter(const QueryFilter& filter);
    void setFilter(std::size_t index, const QueryFilter& filter);

    // Places the agent at the point of the mesh closest to its position, or leaves it `Invalid` when the mesh is not near. Throws `std::invalid_argument` for a full crowd or options out of their range.
    AgentId addAgent(const AgentOptions& options);
    bool removeAgent(AgentId id);
    [[nodiscard]] bool hasAgent(AgentId id) const noexcept;
    [[nodiscard]] std::size_t getAgentCount() const noexcept;

    // The ids of the agents in the order that the bulk reads and writes use, which changes only when an agent is added or removed.
    [[nodiscard]] const std::vector<AgentId>& getAgents() const noexcept;

    // Sends the agent to a point, which snaps to the mesh near it. Returns false when no polygon is near the point.
    bool setTarget(AgentId id, math::Vec3 target);
    void setVelocity(AgentId id, math::Vec3 velocity);
    void clearTarget(AgentId id);

    // Moves the agent to the point of the mesh closest to the position at once, and plans its target again from there. Returns false when no polygon is near the point.
    bool setPosition(AgentId id, math::Vec3 position);

    // Lets an agent that waits at the start of a link with `Manual` traversal go on from its end.
    bool completeLink(AgentId id);

    void step(float deltaSeconds, core::JobSystem* jobs = nullptr);

    [[nodiscard]] math::Vec3 getPosition(AgentId id) const;
    [[nodiscard]] math::Vec3 getVelocity(AgentId id) const;
    [[nodiscard]] AgentState getState(AgentId id) const;

    // Tells whether the agent has a target and the path to it ends short of it, because the target is cut off from the agent.
    [[nodiscard]] bool isPartial(AgentId id) const;
    [[nodiscard]] std::optional<math::Vec3> getTarget(AgentId id) const;

    // These write three numbers for each agent in the order of `getAgents`, and read three numbers for each one, from the span on.
    void readPositions(std::span<float> values) const;
    void readVelocities(std::span<float> values) const;
    void setTargets(std::span<const float> values);

    [[nodiscard]] const std::vector<LinkEntry>& getLinkEntries() const noexcept;

    static const Options kDefaultOptions;

  private:
    struct State;

    std::unique_ptr<State> state;
};

} // namespace haylen::navigation3d

namespace haylen::core {

template <> struct EnumNames<navigation3d::Crowd::AgentState> {
    using AgentState = navigation3d::Crowd::AgentState;

    static constexpr std::array<std::pair<std::string_view, AgentState>, 3> kNames{{{"walking", AgentState::Walking}, {"onLink", AgentState::OnLink}, {"invalid", AgentState::Invalid}}};

    static std::optional<AgentState> fromName(std::string_view name) {
        const auto found = std::ranges::find(kNames, name, &std::pair<std::string_view, AgentState>::first);
        return found != kNames.end() ? std::optional(found->second) : std::nullopt;
    }
    static std::string_view name(AgentState value) {
        return std::ranges::find(kNames, value, &std::pair<std::string_view, AgentState>::second)->first;
    }
};

template <> struct EnumNames<navigation3d::Crowd::Avoidance> {
    using Avoidance = navigation3d::Crowd::Avoidance;

    static constexpr std::array<std::pair<std::string_view, Avoidance>, 4> kNames{{{"none", Avoidance::None}, {"low", Avoidance::Low}, {"medium", Avoidance::Medium}, {"high", Avoidance::High}}};

    static std::optional<Avoidance> fromName(std::string_view name) {
        const auto found = std::ranges::find(kNames, name, &std::pair<std::string_view, Avoidance>::first);
        return found != kNames.end() ? std::optional(found->second) : std::nullopt;
    }
    static std::string_view name(Avoidance value) {
        return std::ranges::find(kNames, value, &std::pair<std::string_view, Avoidance>::second)->first;
    }
};

template <> struct EnumNames<navigation3d::Crowd::LinkTraversal> {
    using LinkTraversal = navigation3d::Crowd::LinkTraversal;

    static constexpr std::array<std::pair<std::string_view, LinkTraversal>, 2> kNames{{{"automatic", LinkTraversal::Automatic}, {"manual", LinkTraversal::Manual}}};

    static std::optional<LinkTraversal> fromName(std::string_view name) {
        const auto found = std::ranges::find(kNames, name, &std::pair<std::string_view, LinkTraversal>::first);
        return found != kNames.end() ? std::optional(found->second) : std::nullopt;
    }
    static std::string_view name(LinkTraversal value) {
        return std::ranges::find(kNames, value, &std::pair<std::string_view, LinkTraversal>::second)->first;
    }
};

} // namespace haylen::core
