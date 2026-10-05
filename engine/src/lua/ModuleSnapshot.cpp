#include "lua/ModuleSnapshot.hpp"

namespace haylen::lua {

bool ModuleSnapshot::isMergeable(lua_State* L, int index) {
    if (lua_type(L, index) != LUA_TTABLE) {
        return false;
    }
    const int table = lua_absindex(L, index);
    if (lua_getmetatable(L, table) == 0) {
        return true;
    }
    lua_pop(L, 1);
    lua_pushliteral(L, "__index");
    lua_rawget(L, table);
    const bool prototype = lua_rawequal(L, -1, table) != 0;
    lua_pop(L, 1);
    return prototype;
}

void ModuleSnapshot::push(lua_State* L, int value, int upvalues, int globals, int creators, std::string_view path) {
    const int valueIndex = lua_absindex(L, value);
    const int upvaluesIndex = lua_absindex(L, upvalues);
    const int globalsIndex = lua_absindex(L, globals);
    const int creatorsIndex = lua_absindex(L, creators);
    lua_createtable(L, 0, 3);
    const int snapshot = lua_gettop(L);
    lua_newtable(L);
    const int copies = lua_gettop(L);
    int budget = kMaxEntries;
    copy(L, valueIndex, copies, 0, budget);
    lua_setfield(L, snapshot, "value");
    copy(L, upvaluesIndex, copies, 0, budget);
    lua_setfield(L, snapshot, "upvalues");
    copy(L, globalsIndex, copies, 0, budget);
    lua_setfield(L, snapshot, "globals");
    claim(L, copies, creatorsIndex, path);
    lua_pop(L, 1);
}

// Every original table that the snapshot copied and no module claimed yet becomes a table of the module.
void ModuleSnapshot::claim(lua_State* L, int copies, int creators, std::string_view path) {
    lua_pushnil(L);
    while (lua_next(L, copies) != 0) {
        lua_pop(L, 1);
        lua_pushvalue(L, -1);
        if (lua_rawget(L, creators) == LUA_TNIL) {
            lua_pushvalue(L, -2);
            lua_pushlstring(L, path.data(), path.size());
            lua_rawset(L, creators);
        }
        lua_pop(L, 1);
    }
}

// Keys keep their identity, because the live tables a snapshot is compared with use the same keys.
void ModuleSnapshot::copy(lua_State* L, int value, int copies, int depth, int& budget) {
    luaL_checkstack(L, 6, "The snapshot of a module is too deep.");
    if (!isMergeable(L, value) || depth > kMaxDepth || budget <= 0) {
        lua_pushvalue(L, value);
        return;
    }
    lua_pushvalue(L, value);
    if (lua_rawget(L, copies) != LUA_TNIL) {
        return;
    }
    lua_pop(L, 1);

    lua_newtable(L);
    const int result = lua_gettop(L);
    lua_pushvalue(L, value);
    lua_pushvalue(L, result);
    lua_rawset(L, copies);
    lua_pushnil(L);
    while (lua_next(L, value) != 0) {
        --budget;
        copy(L, lua_gettop(L), copies, depth + 1, budget);
        lua_pushvalue(L, -3);
        lua_insert(L, -2);
        lua_rawset(L, result);
        lua_pop(L, 1);
    }
}

bool ModuleSnapshot::equals(lua_State* L, int live, int base) {
    const int liveIndex = lua_absindex(L, live);
    const int baseIndex = lua_absindex(L, base);
    lua_newtable(L);
    int budget = kMaxEntries;
    const bool same = compare(L, liveIndex, baseIndex, lua_gettop(L), 0, budget);
    lua_pop(L, 1);
    return same;
}

// A pair of tables met again counts as equal, which ends cycles, and a table past the bounds counts as changed.
bool ModuleSnapshot::compare(lua_State* L, int live, int base, int visited, int depth, int& budget) {
    luaL_checkstack(L, 6, "The snapshot of a module is too deep.");
    if (lua_rawequal(L, live, base) != 0) {
        return true;
    }
    if (lua_type(L, base) != LUA_TTABLE || !isMergeable(L, live) || depth > kMaxDepth || budget <= 0) {
        return false;
    }
    lua_pushvalue(L, base);
    const bool met = lua_rawget(L, visited) != LUA_TNIL;
    lua_pop(L, 1);
    if (met) {
        return true;
    }
    lua_pushvalue(L, base);
    lua_pushvalue(L, live);
    lua_rawset(L, visited);

    int count = 0;
    lua_pushnil(L);
    while (lua_next(L, base) != 0) {
        --budget;
        ++count;
        lua_pushvalue(L, -2);
        lua_rawget(L, live);
        const bool same = compare(L, lua_gettop(L), lua_gettop(L) - 1, visited, depth + 1, budget);
        lua_pop(L, 2);
        if (!same) {
            lua_pop(L, 1);
            return false;
        }
    }
    lua_pushnil(L);
    while (lua_next(L, live) != 0) {
        lua_pop(L, 1);
        if (--count < 0) {
            lua_pop(L, 1);
            return false;
        }
    }
    return count == 0;
}

} // namespace haylen::lua
