#pragma once

#include <lua.hpp>

#include <array>
#include <string_view>

#include "haylen/2d/navigation/Crowd.hpp"
#include "haylen/2d/navigation/SteeringAgent.hpp"
#include "haylen/2d/navigation/Wanderer.hpp"

namespace haylen::navigation2d {

// Installs the `SteeringAgent` and `Crowd` classes of `haylen.navigation2d`.
class AgentLua final {
  public:
    static void install(lua_State* L);

    // Sets `newAgent` and `newCrowd` on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 8> kAgentFields{"x", "y", "maxSpeed", "maxForce", "wanderDistance", "wanderRadius", "wanderJitter", "seed"};
    static constexpr std::array<std::string_view, 3> kFlockingFields{"separation", "alignment", "cohesion"};
    static constexpr std::array<std::string_view, 10> kCrowdAgentFields{"x", "y", "vx", "vy", "radius", "maxSpeed", "neighborDistance", "maxNeighbors", "timeHorizon", "obstacleTimeHorizon"};

    [[nodiscard]] static SteeringAgent& checkAgent(lua_State* L);
    static int newAgent(lua_State* L);
    static int agentSeek(lua_State* L);
    static int agentFlee(lua_State* L);
    static int agentArrive(lua_State* L);
    static int agentSeparation(lua_State* L);
    static int agentAlignment(lua_State* L);
    static int agentCohesion(lua_State* L);
    static int agentAvoid(lua_State* L);
    static int agentWander(lua_State* L);
    static int agentApply(lua_State* L);
    template <float Wanderer::Settings::* Field> static int agentGetWander(lua_State* L);
    template <float Wanderer::Settings::* Field> static int agentSetWander(lua_State* L);

    [[nodiscard]] static Crowd& checkCrowd(lua_State* L);
    [[nodiscard]] static std::uint32_t readAgentId(lua_State* L);
    static int newCrowd(lua_State* L);
    static int crowdAddAgent(lua_State* L);
    static int crowdRemoveAgent(lua_State* L);
    static int crowdHasAgent(lua_State* L);
    static int crowdAddObstacle(lua_State* L);
    static int crowdClearObstacles(lua_State* L);
    static int crowdPosition(lua_State* L);
    static int crowdSetPosition(lua_State* L);
    static int crowdVelocity(lua_State* L);
    static int crowdRadius(lua_State* L);
    static int crowdMaxSpeed(lua_State* L);
    static int crowdSetPreferredVelocity(lua_State* L);
    static int crowdPreferredVelocity(lua_State* L);
    static int crowdSetTarget(lua_State* L);
    static int crowdClearTarget(lua_State* L);
    static int crowdTarget(lua_State* L);
    static int crowdStep(lua_State* L);
    static int crowdAgentCount(lua_State* L);
    template <float Crowd::Flocking::* Field> static int crowdGetFlocking(lua_State* L);
    template <float Crowd::Flocking::* Field> static int crowdSetFlocking(lua_State* L);
};

} // namespace haylen::navigation2d
