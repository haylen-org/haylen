#include "2d/navigation/AgentBinding.hpp"

#include <cstdint>
#include <optional>
#include <vector>

#include "2d/navigation/ScriptedAgent.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/Type.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/math/Circle.hpp"

namespace haylen::lua {

template <> struct Type<lua::navigation2d::ScriptedAgent> {
    static constexpr const char* name = "haylen.SteeringAgent";
    using Storage = lua::navigation2d::ScriptedAgent;
};

template <> struct Type<haylen::navigation2d::Crowd> {
    static constexpr const char* name = "haylen.Crowd";
    using Storage = haylen::navigation2d::Crowd;
};

} // namespace haylen::lua

namespace haylen::lua::navigation2d {

haylen::navigation2d::SteeringAgent& AgentBinding::checkAgent(lua_State* L) {
    return lua::Userdata::check<ScriptedAgent>(L, 1).agent;
}

// Creates an agent with `newAgent({x, y, maxSpeed, maxForce, wanderDistance, wanderRadius, wanderJitter, seed})`.
int AgentBinding::newAgent(lua_State* L) {
    haylen::navigation2d::SteeringAgent agent;
    haylen::navigation2d::Wanderer::Settings wander;
    lua_Integer seed = 1;
    if (!lua_isnoneornil(L, 1)) {
        luaL_checktype(L, 1, LUA_TTABLE);
        lua::Table::checkFields(L, 1, {kAgentFields});
        lua::Table::readField(L, 1, "x", agent.position.x);
        lua::Table::readField(L, 1, "y", agent.position.y);
        lua::Table::readField(L, 1, "maxSpeed", agent.maxSpeed);
        lua::Table::readField(L, 1, "maxForce", agent.maxForce);
        lua::Table::readField(L, 1, "wanderDistance", wander.distance);
        lua::Table::readField(L, 1, "wanderRadius", wander.radius);
        lua::Table::readField(L, 1, "wanderJitter", wander.jitter);
        lua::Table::readField(L, 1, "seed", seed);
    }

    lua::Userdata::emplace<ScriptedAgent>(L, ScriptedAgent{.agent = agent, .wanderer = haylen::navigation2d::Wanderer(static_cast<std::uint64_t>(seed), wander)});
    return 1;
}

int AgentBinding::agentSeek(lua_State* L) {
    lua::Stack::push(L, checkAgent(L).seek(lua::Stack::read<haylen::math::Vec2>(L, 2)));
    return 1;
}

int AgentBinding::agentFlee(lua_State* L) {
    lua::Stack::push(L, checkAgent(L).flee(lua::Stack::read<haylen::math::Vec2>(L, 2)));
    return 1;
}

int AgentBinding::agentArrive(lua_State* L) {
    lua::Stack::push(L, checkAgent(L).arrive(lua::Stack::read<haylen::math::Vec2>(L, 2), lua::Stack::read<float>(L, 3)));
    return 1;
}

int AgentBinding::agentSeparation(lua_State* L) {
    lua::Stack::push(L, checkAgent(L).separation(lua::Stack::read<std::vector<haylen::math::Vec2>>(L, 2), lua::Stack::read<float>(L, 3)));
    return 1;
}

int AgentBinding::agentAlignment(lua_State* L) {
    lua::Stack::push(L, checkAgent(L).alignment(lua::Stack::read<std::vector<haylen::math::Vec2>>(L, 2)));
    return 1;
}

int AgentBinding::agentCohesion(lua_State* L) {
    lua::Stack::push(L, checkAgent(L).cohesion(lua::Stack::read<std::vector<haylen::math::Vec2>>(L, 2)));
    return 1;
}

// Steers around circles with `avoid(circles, lookAhead)`, where each circle is `{center, radius}`.
int AgentBinding::agentAvoid(lua_State* L) {
    lua::Stack::push(L, checkAgent(L).avoid(lua::Stack::read<std::vector<haylen::math::Circle>>(L, 2), lua::Stack::read<float>(L, 3)));
    return 1;
}

int AgentBinding::agentWander(lua_State* L) {
    ScriptedAgent& self = lua::Userdata::check<ScriptedAgent>(L, 1);
    lua::Stack::push(L, self.wanderer.steer(self.agent, lua::Stack::read<float>(L, 2)));
    return 1;
}

int AgentBinding::agentApply(lua_State* L) {
    checkAgent(L).apply(lua::Stack::read<haylen::math::Vec2>(L, 2), lua::Stack::read<float>(L, 3));
    return 0;
}

template <float haylen::navigation2d::Wanderer::Settings::* Field> int AgentBinding::agentGetWander(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedAgent>(L, 1).wanderer.getSettings().*Field);
    return 1;
}

template <float haylen::navigation2d::Wanderer::Settings::* Field> int AgentBinding::agentSetWander(lua_State* L) {
    haylen::navigation2d::Wanderer& wanderer = lua::Userdata::check<ScriptedAgent>(L, 1).wanderer;
    haylen::navigation2d::Wanderer::Settings settings = wanderer.getSettings();
    settings.*Field = lua::Stack::read<float>(L, 3);
    wanderer.setSettings(settings);
    return 0;
}

haylen::navigation2d::Crowd& AgentBinding::checkCrowd(lua_State* L) {
    return lua::Userdata::check<haylen::navigation2d::Crowd>(L, 1);
}

haylen::navigation2d::Crowd::AgentId AgentBinding::readAgentId(lua_State* L) {
    return lua::Stack::read<haylen::navigation2d::Crowd::AgentId>(L, 2);
}

// Creates a crowd with `newCrowd([{separation = 0, alignment = 0, cohesion = 0}])`.
int AgentBinding::newCrowd(lua_State* L) {
    haylen::navigation2d::Crowd::Flocking flocking;
    if (!lua_isnoneornil(L, 1)) {
        luaL_checktype(L, 1, LUA_TTABLE);
        lua::Table::checkFields(L, 1, {kFlockingFields});
        lua::Table::readField(L, 1, "separation", flocking.separation);
        lua::Table::readField(L, 1, "alignment", flocking.alignment);
        lua::Table::readField(L, 1, "cohesion", flocking.cohesion);
    }

    lua::Userdata::emplace<haylen::navigation2d::Crowd>(L, flocking);
    return 1;
}

// Adds an agent with `addAgent({x, y, vx, vy, radius, maxSpeed, neighborDistance, maxNeighbors, timeHorizon, obstacleTimeHorizon})` and returns its id.
int AgentBinding::crowdAddAgent(lua_State* L) {
    haylen::navigation2d::Crowd& crowd = checkCrowd(L);
    haylen::navigation2d::Crowd::AgentOptions options;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kCrowdAgentFields});
        lua::Table::readField(L, 2, "x", options.position.x);
        lua::Table::readField(L, 2, "y", options.position.y);
        lua::Table::readField(L, 2, "vx", options.velocity.x);
        lua::Table::readField(L, 2, "vy", options.velocity.y);
        lua::Table::readField(L, 2, "radius", options.radius);
        lua::Table::readField(L, 2, "maxSpeed", options.maxSpeed);
        lua::Table::readField(L, 2, "neighborDistance", options.neighborDistance);
        lua::Table::readField(L, 2, "maxNeighbors", options.maxNeighbors);
        lua::Table::readField(L, 2, "timeHorizon", options.timeHorizon);
        lua::Table::readField(L, 2, "obstacleTimeHorizon", options.obstacleTimeHorizon);
    }

    lua::Stack::push(L, crowd.addAgent(options));
    return 1;
}

int AgentBinding::crowdRemoveAgent(lua_State* L) {
    lua::Stack::push(L, checkCrowd(L).removeAgent(readAgentId(L)));
    return 1;
}

int AgentBinding::crowdHasAgent(lua_State* L) {
    lua::Stack::push(L, checkCrowd(L).hasAgent(readAgentId(L)));
    return 1;
}

int AgentBinding::crowdAddObstacle(lua_State* L) {
    checkCrowd(L).addObstacle(lua::Stack::read<std::vector<haylen::math::Vec2>>(L, 2));
    return 0;
}

int AgentBinding::crowdClearObstacles(lua_State* L) {
    checkCrowd(L).clearObstacles();
    return 0;
}

int AgentBinding::crowdPosition(lua_State* L) {
    lua::Stack::push(L, checkCrowd(L).getPosition(readAgentId(L)));
    return 1;
}

int AgentBinding::crowdSetPosition(lua_State* L) {
    checkCrowd(L).setPosition(readAgentId(L), {lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4)});
    return 0;
}

int AgentBinding::crowdVelocity(lua_State* L) {
    lua::Stack::push(L, checkCrowd(L).getVelocity(readAgentId(L)));
    return 1;
}

int AgentBinding::crowdRadius(lua_State* L) {
    lua::Stack::push(L, checkCrowd(L).getRadius(readAgentId(L)));
    return 1;
}

int AgentBinding::crowdMaxSpeed(lua_State* L) {
    lua::Stack::push(L, checkCrowd(L).getMaxSpeed(readAgentId(L)));
    return 1;
}

int AgentBinding::crowdSetPreferredVelocity(lua_State* L) {
    checkCrowd(L).setPreferredVelocity(readAgentId(L), {lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4)});
    return 0;
}

int AgentBinding::crowdPreferredVelocity(lua_State* L) {
    lua::Stack::push(L, checkCrowd(L).getPreferredVelocity(readAgentId(L)));
    return 1;
}

int AgentBinding::crowdSetTarget(lua_State* L) {
    checkCrowd(L).setTarget(readAgentId(L), {lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4)});
    return 0;
}

int AgentBinding::crowdClearTarget(lua_State* L) {
    checkCrowd(L).clearTarget(readAgentId(L));
    return 0;
}

int AgentBinding::crowdTarget(lua_State* L) {
    lua::Stack::push(L, checkCrowd(L).getTarget(readAgentId(L)));
    return 1;
}

// Moves every agent with `step(dt)`, spreading the work over the engine job system.
int AgentBinding::crowdStep(lua_State* L) {
    checkCrowd(L).step(lua::Stack::read<float>(L, 2), &lua::Runtime::getEngine(L).getJobs());
    return 0;
}

int AgentBinding::crowdAgentCount(lua_State* L) {
    lua::Stack::push(L, checkCrowd(L).getAgentCount());
    return 1;
}

template <float haylen::navigation2d::Crowd::Flocking::* Field> int AgentBinding::crowdGetFlocking(lua_State* L) {
    lua::Stack::push(L, checkCrowd(L).getFlocking().*Field);
    return 1;
}

template <float haylen::navigation2d::Crowd::Flocking::* Field> int AgentBinding::crowdSetFlocking(lua_State* L) {
    haylen::navigation2d::Crowd& crowd = checkCrowd(L);
    haylen::navigation2d::Crowd::Flocking flocking = crowd.getFlocking();
    flocking.*Field = lua::Stack::read<float>(L, 3);
    crowd.setFlocking(flocking);
    return 0;
}

void AgentBinding::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"newAgent", &lua::Binding::native<&newAgent>},
        {"newCrowd", &lua::Binding::native<&newCrowd>},
        {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

void AgentBinding::install(lua_State* L) {
    lua::ClassBuilder<ScriptedAgent>(L).nestedField<&ScriptedAgent::agent, &haylen::navigation2d::SteeringAgent::position>("position").nestedField<&ScriptedAgent::agent, &haylen::navigation2d::SteeringAgent::velocity>("velocity").nestedField<&ScriptedAgent::agent, &haylen::navigation2d::SteeringAgent::maxSpeed>("maxSpeed").nestedField<&ScriptedAgent::agent, &haylen::navigation2d::SteeringAgent::maxForce>("maxForce").property("wanderDistance", &agentGetWander<&haylen::navigation2d::Wanderer::Settings::distance>, &lua::Binding::native<&agentSetWander<&haylen::navigation2d::Wanderer::Settings::distance>>).property("wanderRadius", &agentGetWander<&haylen::navigation2d::Wanderer::Settings::radius>, &lua::Binding::native<&agentSetWander<&haylen::navigation2d::Wanderer::Settings::radius>>).property("wanderJitter", &agentGetWander<&haylen::navigation2d::Wanderer::Settings::jitter>, &lua::Binding::native<&agentSetWander<&haylen::navigation2d::Wanderer::Settings::jitter>>).function("seek", &lua::Binding::native<&agentSeek>).function("flee", &lua::Binding::native<&agentFlee>).function("arrive", &lua::Binding::native<&agentArrive>).function("separation", &lua::Binding::native<&agentSeparation>).function("alignment", &lua::Binding::native<&agentAlignment>).function("cohesion", &lua::Binding::native<&agentCohesion>).function("avoid", &lua::Binding::native<&agentAvoid>).function("wander", &lua::Binding::native<&agentWander>).function("apply", &lua::Binding::native<&agentApply>).install();
    lua::ClassBuilder<haylen::navigation2d::Crowd>(L).function("addAgent", &lua::Binding::native<&crowdAddAgent>).function("removeAgent", &lua::Binding::native<&crowdRemoveAgent>).function("hasAgent", &lua::Binding::native<&crowdHasAgent>).function("addObstacle", &lua::Binding::native<&crowdAddObstacle>).function("clearObstacles", &lua::Binding::native<&crowdClearObstacles>).function("position", &lua::Binding::native<&crowdPosition>).function("setPosition", &lua::Binding::native<&crowdSetPosition>).function("velocity", &lua::Binding::native<&crowdVelocity>).function("radius", &lua::Binding::native<&crowdRadius>).function("maxSpeed", &lua::Binding::native<&crowdMaxSpeed>).function("setPreferredVelocity", &lua::Binding::native<&crowdSetPreferredVelocity>).function("preferredVelocity", &lua::Binding::native<&crowdPreferredVelocity>).function("setTarget", &lua::Binding::native<&crowdSetTarget>).function("clearTarget", &lua::Binding::native<&crowdClearTarget>).function("target", &lua::Binding::native<&crowdTarget>).function("step", &lua::Binding::native<&crowdStep>).property("agentCount", &crowdAgentCount).property("separation", &crowdGetFlocking<&haylen::navigation2d::Crowd::Flocking::separation>, &lua::Binding::native<&crowdSetFlocking<&haylen::navigation2d::Crowd::Flocking::separation>>).property("alignment", &crowdGetFlocking<&haylen::navigation2d::Crowd::Flocking::alignment>, &lua::Binding::native<&crowdSetFlocking<&haylen::navigation2d::Crowd::Flocking::alignment>>).property("cohesion", &crowdGetFlocking<&haylen::navigation2d::Crowd::Flocking::cohesion>, &lua::Binding::native<&crowdSetFlocking<&haylen::navigation2d::Crowd::Flocking::cohesion>>).install();
}

} // namespace haylen::lua::navigation2d
