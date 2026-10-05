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
    const int type = lua_type(L, index);
    if (type != LUA_TTABLE && type != LUA_TFUNCTION && type != LUA_TUSERDATA && type != LUA_TTHREAD) {
        return;
    }
    if (!seen.insert(lua_topointer(L, index)).second) {
        return;
    }
    lua_pushvalue(L, index);
    lua_rawseti(L, work, ++pending);
}

bool ValueRemap::pushReplacement(int index) {
    const int type = lua_type(L, index);
    if (type != LUA_TTABLE && type != LUA_TFUNCTION) {
        return false;
    }
    lua_pushvalue(L, index);
    if (lua_rawget(L, replacements) == LUA_TNIL) {
        lua_pop(L, 1);
        return false;
    }
    return true;
}

// A metatable that is a patched table, or a class that inherits from one, makes the table an instance whose `reloaded` hook runs.
bool ValueRemap::isInstanceOfPatched(int metatable) {
    lua_pushvalue(L, metatable);
    for (int depth = 0; depth < kMaxSuperDepth && lua_type(L, -1) == LUA_TTABLE; ++depth) {
        lua_pushvalue(L, -1);
        if (lua_rawget(L, classes) != LUA_TNIL) {
            lua_pop(L, 2);
            return true;
        }
        lua_pop(L, 1);
        lua_pushliteral(L, "super");
        lua_rawget(L, -2);
        lua_remove(L, -2);
    }
    lua_pop(L, 1);
    return false;
}

// Values change in place while the traversal goes on, which Lua allows for existing keys, and keys change after it.
void ValueRemap::visitTable(int index) {
    luaL_checkstack(L, 8, "The Lua heap is too deep to remap.");
    if (lua_getmetatable(L, index) != 0) {
        if (isInstanceOfPatched(lua_gettop(L))) {
            lua_pushvalue(L, index);
            lua_rawseti(L, found, ++foundCount);
        }
        enqueue(lua_gettop(L));
        lua_pop(L, 1);
    }

    lua_newtable(L);
    const int renamed = lua_gettop(L);
    int renamedCount = 0;
    lua_pushnil(L);
    while (lua_next(L, index) != 0) {
        const int value = lua_gettop(L);
        if (pushReplacement(value)) {
            lua_pushvalue(L, value - 1);
            lua_pushvalue(L, -2);
            lua_rawset(L, index);
            enqueue(lua_gettop(L));
            lua_pop(L, 1);
        } else {
            enqueue(value);
        }
        if (pushReplacement(value - 1)) {
            lua_pop(L, 1);
            lua_pushvalue(L, value - 1);
            lua_rawseti(L, renamed, ++renamedCount);
        }
        enqueue(value - 1);
        lua_pop(L, 1);
    }

    for (int position = 1; position <= renamedCount; ++position) {
        lua_rawgeti(L, renamed, position);
        const int key = lua_gettop(L);
        (void)pushReplacement(key);
        enqueue(key + 1);
        lua_pushvalue(L, key);
        lua_rawget(L, index);
        lua_pushvalue(L, key);
        lua_pushnil(L);
        lua_rawset(L, index);

        // A table that already holds the new key keeps its value there.
        lua_pushvalue(L, key + 1);
        if (lua_rawget(L, index) == LUA_TNIL) {
            lua_pop(L, 1);
            lua_rawset(L, index);
        }
        lua_settop(L, key - 1);
    }
    lua_pop(L, 1);
}

void ValueRemap::visitFunction(int index) {
    luaL_checkstack(L, 4, "The Lua heap is too deep to remap.");
    for (int upvalue = 1; lua_getupvalue(L, index, upvalue) != nullptr; ++upvalue) {
        if (pushReplacement(lua_gettop(L))) {
            lua_pushvalue(L, -1);
            lua_setupvalue(L, index, upvalue);
            lua_remove(L, -2);
        }
        enqueue(lua_gettop(L));
        lua_pop(L, 1);
    }
}

void ValueRemap::visitUserdata(int index) {
    luaL_checkstack(L, 4, "The Lua heap is too deep to remap.");
    if (lua_getmetatable(L, index) != 0) {
        enqueue(lua_gettop(L));
        lua_pop(L, 1);
    }
    for (int slot = 1; lua_getiuservalue(L, index, slot) != LUA_TNONE; ++slot) {
        if (pushReplacement(lua_gettop(L))) {
            lua_pushvalue(L, -1);
            lua_setiuservalue(L, index, slot);
            lua_remove(L, -2);
        }
        enqueue(lua_gettop(L));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

// The functions that a coroutine runs keep their old bodies, while the locals of every level and the values on its stack take the replacements. A coroutine that failed keeps its stack as it was.
void ValueRemap::visitThread(int index) {
    lua_State* thread = lua_tothread(L, index);
    if (thread == L || lua_status(thread) > LUA_YIELD || lua_checkstack(thread, 4) == 0) {
        return;
    }
    luaL_checkstack(L, 4, "The Lua heap is too deep to remap.");
    lua_Debug level{};
    for (int depth = 0; lua_getstack(thread, depth, &level) != 0; ++depth) {
        if (lua_getinfo(thread, "f", &level) != 0) {
            lua_xmove(thread, L, 1);
            enqueue(lua_gettop(L));
            lua_pop(L, 1);
        }
        for (int local = 1; lua_getlocal(thread, &level, local) != nullptr; ++local) {
            lua_xmove(thread, L, 1);
            if (pushReplacement(lua_gettop(L))) {
                lua_pushvalue(L, -1);
                lua_xmove(L, thread, 1);
                lua_setlocal(thread, &level, local);
                lua_remove(L, -2);
            }
            enqueue(lua_gettop(L));
            lua_pop(L, 1);
        }
    }
    const int top = lua_gettop(thread);
    for (int slot = 1; slot <= top; ++slot) {
        lua_pushvalue(thread, slot);
        lua_xmove(thread, L, 1);
        enqueue(lua_gettop(L));
        lua_pop(L, 1);
    }
}

} // namespace haylen::lua
