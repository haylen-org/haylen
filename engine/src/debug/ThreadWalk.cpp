#include "debug/ThreadWalk.hpp"

namespace haylen::debug {

void ThreadWalk::run(lua_State* L, const std::function<void(lua_State* thread)>& visit) {
    const bool collecting = lua_gc(L, LUA_GCISRUNNING) != 0;
    lua_gc(L, LUA_GCSTOP);
    lua_newtable(L);
    ThreadWalk walk(L, lua_gettop(L));
    walk.seen.insert(lua_topointer(L, walk.work));
    walk.seen.insert(L);
    lua_pushvalue(L, LUA_REGISTRYINDEX);
    walk.enqueue(lua_gettop(L));
    lua_pop(L, 1);

    while (walk.pending > 0) {
        lua_rawgeti(L, walk.work, walk.pending);
        lua_pushnil(L);
        lua_rawseti(L, walk.work, walk.pending);
        --walk.pending;
        const int value = lua_gettop(L);
        switch (lua_type(L, value)) {
        case LUA_TTABLE:
            walk.visitTable(value);
            break;
        case LUA_TFUNCTION:
            walk.visitFunction(value);
            break;
        case LUA_TUSERDATA:
            walk.visitUserdata(value);
            break;
        case LUA_TTHREAD: {
            lua_State* thread = lua_tothread(L, value);
            walk.visitThread(thread);
            visit(thread);
            break;
        }
        default:
            break;
        }
        lua_settop(L, value - 1);
    }
    lua_pop(L, 1);
    if (collecting) {
        lua_gc(L, LUA_GCRESTART);
    }
}

void ThreadWalk::enqueue(int index) {
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

void ThreadWalk::visitTable(int index) {
    luaL_checkstack(state, 4, "The Lua heap is too deep to walk.");
    if (lua_getmetatable(state, index) != 0) {
        enqueue(lua_gettop(state));
        lua_pop(state, 1);
    }
    lua_pushnil(state);
    while (lua_next(state, index) != 0) {
        enqueue(-2);
        enqueue(-1);
        lua_pop(state, 1);
    }
}

void ThreadWalk::visitFunction(int index) {
    luaL_checkstack(state, 2, "The Lua heap is too deep to walk.");
    for (int upvalue = 1; lua_getupvalue(state, index, upvalue) != nullptr; ++upvalue) {
        enqueue(-1);
        lua_pop(state, 1);
    }
}

void ThreadWalk::visitUserdata(int index) {
    luaL_checkstack(state, 2, "The Lua heap is too deep to walk.");
    if (lua_getmetatable(state, index) != 0) {
        enqueue(-1);
        lua_pop(state, 1);
    }
    for (int slot = 1; lua_getiuservalue(state, index, slot) != LUA_TNONE; ++slot) {
        enqueue(-1);
        lua_pop(state, 1);
    }
    lua_pop(state, 1);
}

// The functions and locals of every level of a coroutine and the values on its stack hold whatever it reaches, also the coroutine that resumed it last. A coroutine that failed keeps its stack as it was.
void ThreadWalk::visitThread(lua_State* thread) {
    if (lua_status(thread) > LUA_YIELD || lua_checkstack(thread, 2) == 0) {
        return;
    }
    luaL_checkstack(state, 2, "The Lua heap is too deep to walk.");
    lua_Debug level{};
    for (int depth = 0; lua_getstack(thread, depth, &level) != 0; ++depth) {
        if (lua_getinfo(thread, "f", &level) != 0) {
            lua_xmove(thread, state, 1);
            enqueue(-1);
            lua_pop(state, 1);
        }
        for (int local = 1; lua_getlocal(thread, &level, local) != nullptr; ++local) {
            lua_xmove(thread, state, 1);
            enqueue(-1);
            lua_pop(state, 1);
        }
    }
    const int top = lua_gettop(thread);
    for (int slot = 1; slot <= top; ++slot) {
        lua_pushvalue(thread, slot);
        lua_xmove(thread, state, 1);
        enqueue(-1);
        lua_pop(state, 1);
    }
}

} // namespace haylen::debug
