#include "lua/ValueRemap.hpp"

namespace haylen::lua {

void ValueRemap::run(lua_State* L, int replacements, int skipped, int classes) {
    const int replacementsIndex = lua_absindex(L, replacements);
    const int skippedIndex = lua_absindex(L, skipped);
    const int classesIndex = lua_absindex(L, classes);
    lua_newtable(L);
    const int found = lua_gettop(L);
    lua_newtable(L);
    const int work = lua_gettop(L);

    ValueRemap remap(L, replacementsIndex, classesIndex, work, found);
    for (const int index : {replacementsIndex, skippedIndex, classesIndex, found, work}) {
        remap.seen.insert(lua_topointer(L, index));
    }
    remap.seen.insert(L);
    lua_pushvalue(L, LUA_REGISTRYINDEX);
    remap.enqueue(lua_gettop(L));
    lua_pop(L, 1);

    while (remap.pending > 0) {
        lua_rawgeti(L, work, remap.pending);
        lua_pushnil(L);
        lua_rawseti(L, work, remap.pending);
        --remap.pending;
        const int value = lua_gettop(L);
        switch (lua_type(L, value)) {
        case LUA_TTABLE:
            remap.visitTable(value);
            break;
        case LUA_TFUNCTION:
            remap.visitFunction(value);
            break;
        case LUA_TUSERDATA:
            remap.visitUserdata(value);
            break;
        case LUA_TTHREAD:
            remap.visitThread(value);
            break;
        default:
            break;
        }
        lua_settop(L, value - 1);
    }
    lua_pop(L, 1);
}

void ValueRemap::enqueue(int index) {
    const int type = lua_type(state, index);
    if (type != LUA_TTABLE && type != LUA_TFUNCTION && type != LUA_TUSERDATA && type != LUA_TTHREAD) {
        return;
    }
    if (!seen.insert(lua_topointer(state, index)).second) {
        return;
    }
    lua_pushvalue(state, index);
    lua_rawseti(state, work, ++pending);
}

bool ValueRemap::pushReplacement(int index) {
    const int type = lua_type(state, index);
    if (type != LUA_TTABLE && type != LUA_TFUNCTION) {
        return false;
    }
    lua_pushvalue(state, index);
    if (lua_rawget(state, replacements) == LUA_TNIL) {
        lua_pop(state, 1);
        return false;
    }
    return true;
}

// A metatable that is a patched table, or a class that inherits from one, makes the table an instance whose `reloaded` hook runs.
bool ValueRemap::isInstanceOfPatched(int metatable) {
    lua_pushvalue(state, metatable);
    for (int depth = 0; depth < kMaxSuperDepth && lua_type(state, -1) == LUA_TTABLE; ++depth) {
        lua_pushvalue(state, -1);
        if (lua_rawget(state, classes) != LUA_TNIL) {
            lua_pop(state, 2);
            return true;
        }
        lua_pop(state, 1);
        lua_pushliteral(state, "super");
        lua_rawget(state, -2);
        lua_remove(state, -2);
    }
    lua_pop(state, 1);
    return false;
}

// Values change in place while the traversal goes on, which Lua allows for existing keys, and keys change after it. A staging table that the new code made the metatable of a table gives way to its live table, like any other value.
void ValueRemap::visitTable(int index) {
    luaL_checkstack(state, 8, "The Lua heap is too deep to remap.");
    if (lua_getmetatable(state, index) != 0) {
        const int metatable = lua_gettop(state);
        if (pushReplacement(metatable)) {
            lua_pushvalue(state, -1);
            lua_setmetatable(state, index);
            lua_replace(state, metatable);
        }
        if (isInstanceOfPatched(metatable)) {
            lua_pushvalue(state, index);
            lua_rawseti(state, found, ++foundCount);
        }
        enqueue(metatable);
        lua_pop(state, 1);
    }

    lua_newtable(state);
    const int renamed = lua_gettop(state);
    int renamedCount = 0;
    lua_pushnil(state);
    while (lua_next(state, index) != 0) {
        const int value = lua_gettop(state);
        if (pushReplacement(value)) {
            lua_pushvalue(state, value - 1);
            lua_pushvalue(state, -2);
            lua_rawset(state, index);
            enqueue(lua_gettop(state));
            lua_pop(state, 1);
        } else {
            enqueue(value);
        }
        if (pushReplacement(value - 1)) {
            lua_pop(state, 1);
            lua_pushvalue(state, value - 1);
            lua_rawseti(state, renamed, ++renamedCount);
        }
        enqueue(value - 1);
        lua_pop(state, 1);
    }

    for (int position = 1; position <= renamedCount; ++position) {
        lua_rawgeti(state, renamed, position);
        const int key = lua_gettop(state);
        (void)pushReplacement(key);
        enqueue(key + 1);
        lua_pushvalue(state, key);
        lua_rawget(state, index);
        lua_pushvalue(state, key);
        lua_pushnil(state);
        lua_rawset(state, index);

        // A table that already holds the new key keeps its value there.
        lua_pushvalue(state, key + 1);
        if (lua_rawget(state, index) == LUA_TNIL) {
            lua_pop(state, 1);
            lua_rawset(state, index);
        }
        lua_settop(state, key - 1);
    }
    lua_pop(state, 1);
}

void ValueRemap::visitFunction(int index) {
    luaL_checkstack(state, 4, "The Lua heap is too deep to remap.");
    for (int upvalue = 1; lua_getupvalue(state, index, upvalue) != nullptr; ++upvalue) {
        if (pushReplacement(lua_gettop(state))) {
            lua_pushvalue(state, -1);
            lua_setupvalue(state, index, upvalue);
            lua_remove(state, -2);
        }
        enqueue(lua_gettop(state));
        lua_pop(state, 1);
    }
}

void ValueRemap::visitUserdata(int index) {
    luaL_checkstack(state, 4, "The Lua heap is too deep to remap.");
    if (lua_getmetatable(state, index) != 0) {
        enqueue(lua_gettop(state));
        lua_pop(state, 1);
    }
    for (int slot = 1; lua_getiuservalue(state, index, slot) != LUA_TNONE; ++slot) {
        if (pushReplacement(lua_gettop(state))) {
            lua_pushvalue(state, -1);
            lua_setiuservalue(state, index, slot);
            lua_remove(state, -2);
        }
        enqueue(lua_gettop(state));
        lua_pop(state, 1);
    }
    lua_pop(state, 1);
}

// The functions that a coroutine runs keep their old bodies, while the locals of every level and the values on its stack take the replacements. A coroutine that failed keeps its stack as it was.
void ValueRemap::visitThread(int index) {
    lua_State* thread = lua_tothread(state, index);
    if (thread == state || lua_status(thread) > LUA_YIELD || lua_checkstack(thread, 4) == 0) {
        return;
    }
    luaL_checkstack(state, 4, "The Lua heap is too deep to remap.");
    lua_Debug level{};
    for (int depth = 0; lua_getstack(thread, depth, &level) != 0; ++depth) {
        if (lua_getinfo(thread, "f", &level) != 0) {
            lua_xmove(thread, state, 1);
            enqueue(lua_gettop(state));
            lua_pop(state, 1);
        }
        for (int local = 1; lua_getlocal(thread, &level, local) != nullptr; ++local) {
            lua_xmove(thread, state, 1);
            if (pushReplacement(lua_gettop(state))) {
                lua_pushvalue(state, -1);
                lua_xmove(state, thread, 1);
                lua_setlocal(thread, &level, local);
                lua_remove(state, -2);
            }
            enqueue(lua_gettop(state));
            lua_pop(state, 1);
        }
    }
    const int top = lua_gettop(thread);
    for (int slot = 1; slot <= top; ++slot) {
        lua_pushvalue(thread, slot);
        lua_xmove(thread, state, 1);
        enqueue(lua_gettop(state));
        lua_pop(state, 1);
    }
}

} // namespace haylen::lua
