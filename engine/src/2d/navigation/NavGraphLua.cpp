#include "2d/navigation/NavGraphLua.hpp"

#include <cmath>
#include <optional>
#include <vector>

#include "2d/navigation/ScriptedGraph.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Type.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua {

template <> struct Type<navigation2d::ScriptedGraph> {
    static constexpr const char* name = "haylen.NavGraph";
    using Storage = navigation2d::ScriptedGraph;
};

} // namespace haylen::lua

namespace haylen::navigation2d {

Graph& NavGraphLua::check(lua_State* L) {
    return lua::Userdata::check<ScriptedGraph>(L, 1).graph;
}

std::int64_t NavGraphLua::readId(lua_State* L, int index) {
    return lua::Stack::read<std::int64_t>(L, index);
}

void NavGraphLua::pushIds(lua_State* L, std::span<const std::int64_t> ids) {
    lua_createtable(L, static_cast<int>(ids.size()), 0);
    for (std::size_t index = 0; index < ids.size(); ++index) {
        lua_pushinteger(L, static_cast<lua_Integer>(ids[index]));
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
}

int NavGraphLua::newGraph(lua_State* L) {
    lua::Userdata::emplace<ScriptedGraph>(L);
    return 1;
}

// Adds or moves a point with addPoint(id, x, y[, weight]).
int NavGraphLua::addPoint(lua_State* L) {
    const float weight = lua_isnoneornil(L, 5) ? 1.0F : lua::Stack::read<float>(L, 5);
    check(L).addPoint(readId(L, 2), {lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4)}, weight);
    return 0;
}

int NavGraphLua::removePoint(lua_State* L) {
    lua::Stack::push(L, check(L).removePoint(readId(L, 2)));
    return 1;
}

int NavGraphLua::hasPoint(lua_State* L) {
    lua::Stack::push(L, check(L).hasPoint(readId(L, 2)));
    return 1;
}

int NavGraphLua::getPosition(lua_State* L) {
    lua::Stack::push(L, check(L).getPosition(readId(L, 2)));
    return 1;
}

int NavGraphLua::setPosition(lua_State* L) {
    check(L).setPosition(readId(L, 2), {lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4)});
    return 0;
}

int NavGraphLua::getWeight(lua_State* L) {
    lua::Stack::push(L, check(L).getWeight(readId(L, 2)));
    return 1;
}

int NavGraphLua::setWeight(lua_State* L) {
    check(L).setWeight(readId(L, 2), lua::Stack::read<float>(L, 3));
    return 0;
}

int NavGraphLua::isEnabled(lua_State* L) {
    lua::Stack::push(L, check(L).isEnabled(readId(L, 2)));
    return 1;
}

int NavGraphLua::setEnabled(lua_State* L) {
    check(L).setEnabled(readId(L, 2), lua::Stack::read<bool>(L, 3));
    return 0;
}

// Joins two points with connect(from, to[, bidirectional = true]).
int NavGraphLua::connect(lua_State* L) {
    check(L).connect(readId(L, 2), readId(L, 3), lua_isnoneornil(L, 4) || lua::Stack::read<bool>(L, 4));
    return 0;
}

int NavGraphLua::disconnect(lua_State* L) {
    check(L).disconnect(readId(L, 2), readId(L, 3), lua_isnoneornil(L, 4) || lua::Stack::read<bool>(L, 4));
    return 0;
}

int NavGraphLua::isConnected(lua_State* L) {
    lua::Stack::push(L, check(L).isConnected(readId(L, 2), readId(L, 3)));
    return 1;
}

int NavGraphLua::neighbors(lua_State* L) {
    std::vector<std::int64_t> ids;
    check(L).getNeighbors(readId(L, 2), ids);
    pushIds(L, ids);
    return 1;
}

int NavGraphLua::points(lua_State* L) {
    std::vector<std::int64_t> ids;
    check(L).getPoints(ids);
    pushIds(L, ids);
    return 1;
}

// Returns the id of the point closest to x, y with closest(x, y[, includeDisabled]), or nil for an empty graph.
int NavGraphLua::closest(lua_State* L) {
    const bool includeDisabled = !lua_isnoneornil(L, 4) && lua::Stack::read<bool>(L, 4);
    const std::optional<std::int64_t> id = check(L).getClosestPoint({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)}, includeDisabled);
    lua::Stack::push(L, id);
    return 1;
}

// Finds a path with findPath(from, to) and returns its point ids and cost, or nil when there is none.
int NavGraphLua::findPath(lua_State* L) {
    ScriptedGraph& self = lua::Userdata::check<ScriptedGraph>(L, 1);
    const std::span<const std::int64_t> path = self.search.findPath(self.graph, readId(L, 2), readId(L, 3));
    if (path.empty()) {
        lua_pushnil(L);
        return 1;
    }
    pushIds(L, path);
    lua::Stack::push(L, self.search.getCost());
    return 2;
}

// Returns a table from point id to the cost of reaching it from the source with distances(source), holding only reachable points.
int NavGraphLua::distances(lua_State* L) {
    ScriptedGraph& self = lua::Userdata::check<ScriptedGraph>(L, 1);
    self.search.computeDistances(self.graph, readId(L, 2));
    std::vector<std::int64_t> ids;
    self.graph.getPoints(ids);
    lua_newtable(L);
    for (const std::int64_t id : ids) {
        const float distance = self.search.getDistance(self.graph, id);
        if (std::isfinite(distance)) {
            lua::Stack::push(L, distance);
            lua_rawseti(L, -2, static_cast<lua_Integer>(id));
        }
    }
    return 1;
}

int NavGraphLua::clear(lua_State* L) {
    check(L).clear();
    return 0;
}

int NavGraphLua::size(lua_State* L) {
    lua::Stack::push(L, check(L).size());
    return 1;
}

void NavGraphLua::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"newGraph", &lua::Binding::native<&newGraph>},
        {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

void NavGraphLua::install(lua_State* L) {
    lua::ClassBuilder<ScriptedGraph>(L).function("addPoint", &lua::Binding::native<&addPoint>).function("removePoint", &lua::Binding::native<&removePoint>).function("hasPoint", &lua::Binding::native<&hasPoint>).function("position", &lua::Binding::native<&getPosition>).function("setPosition", &lua::Binding::native<&setPosition>).function("weight", &lua::Binding::native<&getWeight>).function("setWeight", &lua::Binding::native<&setWeight>).function("enabled", &lua::Binding::native<&isEnabled>).function("setEnabled", &lua::Binding::native<&setEnabled>).function("connect", &lua::Binding::native<&connect>).function("disconnect", &lua::Binding::native<&disconnect>).function("connected", &lua::Binding::native<&isConnected>).function("neighbors", &lua::Binding::native<&neighbors>).function("points", &lua::Binding::native<&points>).function("closest", &lua::Binding::native<&closest>).function("findPath", &lua::Binding::native<&findPath>).function("distances", &lua::Binding::native<&distances>).function("clear", &lua::Binding::native<&clear>).property("size", &size).install();
}

} // namespace haylen::navigation2d
