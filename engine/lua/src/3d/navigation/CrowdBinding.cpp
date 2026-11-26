#include "3d/navigation/CrowdBinding.hpp"

#include <lua.hpp>

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include "3d/navigation/NavMeshBinding.hpp"
#include "3d/navigation/NavigationOptions.hpp"
#include "3d/navigation/QueryFilterBinding.hpp"
#include "core/FloatBufferBinding.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/FloatBuffer.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua::navigation3d {

ScriptedCrowd& CrowdBinding::checkSelf(lua_State* L) {
    return lua::Userdata::check<ScriptedCrowd>(L, 1);
}

haylen::navigation3d::Crowd::AgentId CrowdBinding::checkAgent(lua_State* L, int index) {
    return lua::Stack::read<std::uint32_t>(L, index);
}

int CrowdBinding::create(lua_State* L) {
    const std::shared_ptr<haylen::navigation3d::NavMesh>& mesh = lua::Userdata::checkShared<haylen::navigation3d::NavMesh>(L, 1);
    const haylen::navigation3d::Crowd::Options options = NavigationOptions::readCrowd(L, 2);
    lua::Userdata::emplace<ScriptedCrowd>(L, ScriptedCrowd{.mesh = mesh, .crowd = std::make_unique<haylen::navigation3d::Crowd>(*mesh, options), .filters = {haylen::navigation3d::QueryFilter{}}});
    return 1;
}

// Agents that name equal filters share one filter of the crowd.
std::size_t CrowdBinding::findFilter(ScriptedCrowd& self, const haylen::navigation3d::QueryFilter& filter) {
    const auto found = std::ranges::find(self.filters, filter);
    if (found != self.filters.end()) {
        return static_cast<std::size_t>(found - self.filters.begin());
    }

    const std::size_t index = self.crowd->addFilter(filter);
    self.filters.push_back(filter);
    return index;
}

haylen::navigation3d::Crowd::AgentOptions CrowdBinding::readAgent(lua_State* L, ScriptedCrowd& self, int index) {
    haylen::navigation3d::Crowd::AgentOptions options;
    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {NavigationOptions::kAgentFields});
    lua::Table::readField(L, index, "position", options.position);
    lua::Table::readField(L, index, "radius", options.radius);
    lua::Table::readField(L, index, "height", options.height);
    lua::Table::readField(L, index, "maxSpeed", options.maxSpeed);
    lua::Table::readField(L, index, "maxAcceleration", options.maxAcceleration);
    lua::Table::readField(L, index, "collisionRange", options.collisionRange);
    lua::Table::readField(L, index, "pathOptimizationRange", options.pathOptimizationRange);
    lua::Table::readField(L, index, "separationWeight", options.separationWeight);
    lua::Table::readField(L, index, "avoidance", options.avoidance);
    lua::Table::readField(L, index, "anticipateTurns", options.anticipateTurns);
    lua::Table::readField(L, index, "optimizeVisibility", options.optimizeVisibility);
    lua::Table::readField(L, index, "optimizeTopology", options.optimizeTopology);
    lua::Table::readField(L, index, "separation", options.separation);
    lua::Table::readField(L, index, "linkTraversal", options.linkTraversal);
    lua::Table::readField(L, index, "jumpHeight", options.jumpHeight);
    lua::Table::readField(L, index, "climbSpeed", options.climbSpeed);

    lua_getfield(L, index, "filter");
    if (!lua_isnil(L, -1)) {
        options.filter = findFilter(self, lua::Userdata::check<ScriptedFilter>(L, -1).filter);
    }
    lua_pop(L, 1);
    return options;
}

std::span<float> CrowdBinding::readBuffer(lua_State* L, int index) {
    haylen::core::FloatBuffer& buffer = lua::Userdata::check<haylen::core::FloatBuffer>(L, index);
    const lua_Integer first = lua_isnoneornil(L, index + 1) ? 1 : lua::Stack::read<lua_Integer>(L, index + 1);
    luaL_argcheck(L, first >= 1, index + 1, "the values of a buffer are numbered from one");
    const std::span<float> values = buffer.getValues();
    if (static_cast<std::uint64_t>(first - 1) > values.size()) {
        throw std::invalid_argument("The buffer holds " + std::to_string(values.size()) + " values, so it has no value " + std::to_string(first) + ".");
    }

    return values.subspan(static_cast<std::size_t>(first - 1));
}

int CrowdBinding::addAgent(lua_State* L) {
    ScriptedCrowd& self = checkSelf(L);
    const haylen::navigation3d::Crowd::AgentOptions options = readAgent(L, self, 2);
    lua_pushinteger(L, self.crowd->addAgent(options));
    return 1;
}

int CrowdBinding::removeAgent(lua_State* L) {
    lua::Stack::push(L, checkSelf(L).crowd->removeAgent(checkAgent(L, 2)));
    return 1;
}

int CrowdBinding::hasAgent(lua_State* L) {
    lua::Stack::push(L, checkSelf(L).crowd->hasAgent(checkAgent(L, 2)));
    return 1;
}

int CrowdBinding::setTarget(lua_State* L) {
    lua::Stack::push(L, checkSelf(L).crowd->setTarget(checkAgent(L, 2), lua::Stack::read<haylen::math::Vec3>(L, 3)));
    return 1;
}

// Sends every agent at once with `setTargets(buffer[, first])`, three numbers for each agent in the order of `crowd.agents`.
int CrowdBinding::setTargets(lua_State* L) {
    ScriptedCrowd& self = checkSelf(L);
    self.crowd->setTargets(readBuffer(L, 2));
    return 0;
}

int CrowdBinding::setVelocity(lua_State* L) {
    checkSelf(L).crowd->setVelocity(checkAgent(L, 2), lua::Stack::read<haylen::math::Vec3>(L, 3));
    return 0;
}

int CrowdBinding::clearTarget(lua_State* L) {
    checkSelf(L).crowd->clearTarget(checkAgent(L, 2));
    return 0;
}

int CrowdBinding::setPosition(lua_State* L) {
    lua::Stack::push(L, checkSelf(L).crowd->setPosition(checkAgent(L, 2), lua::Stack::read<haylen::math::Vec3>(L, 3)));
    return 1;
}

int CrowdBinding::completeLink(lua_State* L) {
    lua::Stack::push(L, checkSelf(L).crowd->completeLink(checkAgent(L, 2)));
    return 1;
}

// Steps every agent with `step(dt)`, spread over the job system of the engine.
int CrowdBinding::step(lua_State* L) {
    ScriptedCrowd& self = checkSelf(L);
    self.crowd->step(lua::Stack::read<float>(L, 2), &lua::Runtime::getEngine(L).getJobs());
    return 0;
}

int CrowdBinding::position(lua_State* L) {
    lua::Stack::push(L, checkSelf(L).crowd->getPosition(checkAgent(L, 2)));
    return 1;
}

int CrowdBinding::velocity(lua_State* L) {
    lua::Stack::push(L, checkSelf(L).crowd->getVelocity(checkAgent(L, 2)));
    return 1;
}

int CrowdBinding::state(lua_State* L) {
    lua::Stack::push(L, checkSelf(L).crowd->getState(checkAgent(L, 2)));
    return 1;
}

int CrowdBinding::partial(lua_State* L) {
    lua::Stack::push(L, checkSelf(L).crowd->isPartial(checkAgent(L, 2)));
    return 1;
}

int CrowdBinding::target(lua_State* L) {
    const std::optional<haylen::math::Vec3> found = checkSelf(L).crowd->getTarget(checkAgent(L, 2));
    if (!found) {
        lua_pushnil(L);
        return 1;
    }

    lua::Stack::push(L, *found);
    return 1;
}

// Writes the positions of every agent with `readPositions(buffer[, first])`, three numbers for each agent in the order of `crowd.agents`.
int CrowdBinding::readPositions(lua_State* L) {
    checkSelf(L).crowd->readPositions(readBuffer(L, 2));
    return 0;
}

int CrowdBinding::readVelocities(lua_State* L) {
    checkSelf(L).crowd->readVelocities(readBuffer(L, 2));
    return 0;
}

// Lists the agents that reached a link in the last step as `{agent, link, kind, start, end}`.
int CrowdBinding::linkEntries(lua_State* L) {
    const std::vector<haylen::navigation3d::Crowd::LinkEntry>& entries = checkSelf(L).crowd->getLinkEntries();
    lua_createtable(L, static_cast<int>(entries.size()), 0);
    for (std::size_t index = 0; index < entries.size(); ++index) {
        const haylen::navigation3d::Crowd::LinkEntry& entry = entries[index];
        lua_createtable(L, 0, 5);
        lua_pushinteger(L, entry.agent);
        lua_setfield(L, -2, "agent");
        lua_pushinteger(L, entry.link);
        lua_setfield(L, -2, "link");
        lua::Stack::push(L, entry.kind);
        lua_setfield(L, -2, "kind");
        lua::Stack::push(L, entry.start);
        lua_setfield(L, -2, "start");
        lua::Stack::push(L, entry.end);
        lua_setfield(L, -2, "end");
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    return 1;
}

int CrowdBinding::agents(lua_State* L) {
    const std::vector<haylen::navigation3d::Crowd::AgentId>& ids = checkSelf(L).crowd->getAgents();
    lua_createtable(L, static_cast<int>(ids.size()), 0);
    for (std::size_t index = 0; index < ids.size(); ++index) {
        lua_pushinteger(L, ids[index]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    return 1;
}

int CrowdBinding::count(lua_State* L) {
    lua::Stack::push(L, checkSelf(L).crowd->getAgentCount());
    return 1;
}

void CrowdBinding::install(lua_State* L) {
    lua::ClassBuilder<ScriptedCrowd>(L).function("addAgent", &lua::Binding::native<&addAgent>).function("removeAgent", &lua::Binding::native<&removeAgent>).function("hasAgent", &lua::Binding::native<&hasAgent>).function("setTarget", &lua::Binding::native<&setTarget>).function("setTargets", &lua::Binding::native<&setTargets>).function("setVelocity", &lua::Binding::native<&setVelocity>).function("clearTarget", &lua::Binding::native<&clearTarget>).function("setPosition", &lua::Binding::native<&setPosition>).function("completeLink", &lua::Binding::native<&completeLink>).function("step", &lua::Binding::native<&step>).function("position", &lua::Binding::native<&position>).function("velocity", &lua::Binding::native<&velocity>).function("state", &lua::Binding::native<&state>).function("partial", &lua::Binding::native<&partial>).function("target", &lua::Binding::native<&target>).function("readPositions", &lua::Binding::native<&readPositions>).function("readVelocities", &lua::Binding::native<&readVelocities>).function("linkEntries", &lua::Binding::native<&linkEntries>).property("agents", &lua::Binding::native<&agents>).property("count", &lua::Binding::native<&count>).install();
}

} // namespace haylen::lua::navigation3d
