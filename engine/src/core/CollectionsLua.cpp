#include "core/CollectionsLua.hpp"

#include <lua.hpp>

#include "core/FloatBufferLua.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/Type.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua {

template <> struct Type<core::CollectionsLua::Pool> {
    static constexpr const char* name = "haylen.Pool";
    using Storage = core::CollectionsLua::Pool;
};

template <> struct Type<core::CollectionsLua::ValueBuffer> {
    static constexpr const char* name = "haylen.RingBuffer";
    using Storage = core::CollectionsLua::ValueBuffer;
};

} // namespace haylen::lua

namespace haylen::core {

void CollectionsLua::callHook(lua_State* L, const char* name, int object, int first, int count) {
    if (!lua::Userdata::pushFunction(L, 1, name)) {
        return;
    }
    lua_pushvalue(L, object);
    for (int argument = 0; argument < count; ++argument) {
        lua_pushvalue(L, first + argument);
    }
    lua_call(L, count + 1, 0);
}

void CollectionsLua::create(lua_State* L) {
    lua::Userdata::pushField(L, 1, "create");
    lua_call(L, 0, 1);
    if (lua_isnil(L, -1)) {
        luaL_error(L, "The create function of the pool returned nil.");
    }
}

void CollectionsLua::prewarm(lua_State* L, Pool& pool, std::size_t count) {
    lua::Userdata::pushField(L, 1, "idle");
    const int idle = lua_gettop(L);
    while (pool.active + pool.idle < count && (pool.capacity == 0 || pool.active + pool.idle < pool.capacity)) {
        create(L);
        lua_rawseti(L, idle, static_cast<lua_Integer>(++pool.idle));
    }
    lua_pop(L, 1);
}

void CollectionsLua::pushActive(lua_State* L, const Pool& pool) {
    lua::Userdata::pushField(L, 1, "active");
    lua_createtable(L, static_cast<int>(pool.active), 0);
    for (std::size_t slot = 1; slot <= pool.active; ++slot) {
        lua_rawgeti(L, -2, static_cast<lua_Integer>(slot));
        lua_rawseti(L, -2, static_cast<lua_Integer>(slot));
    }
    lua_remove(L, -2);
}

// Active objects fill slots 1 to active of a list, and a released object leaves its slot to the last active object.
bool CollectionsLua::releaseObject(lua_State* L, Pool& pool, int object) {
    lua::Userdata::pushField(L, 1, "slots");
    const int slots = lua_gettop(L);
    lua_pushvalue(L, object);
    if (lua_rawget(L, slots) == LUA_TNIL) {
        lua_settop(L, slots - 1);
        return false;
    }
    const lua_Integer slot = lua_tointeger(L, -1);
    lua::Userdata::pushField(L, 1, "active");
    const int active = lua_gettop(L);

    const auto last = static_cast<lua_Integer>(pool.active);
    lua_rawgeti(L, active, last);
    lua_pushvalue(L, -1);
    lua_rawseti(L, active, slot);
    lua_pushinteger(L, slot);
    lua_rawset(L, slots);
    lua_pushnil(L);
    lua_rawseti(L, active, last);
    lua_pushvalue(L, object);
    lua_pushnil(L);
    lua_rawset(L, slots);
    --pool.active;

    lua::Userdata::pushField(L, 1, "idle");
    lua_pushvalue(L, object);
    lua_rawseti(L, -2, static_cast<lua_Integer>(++pool.idle));
    lua_settop(L, slots - 1);
    callHook(L, "release", object, 0, 0);
    return true;
}

// Creates a pool with newPool({create = function() end, reset = function(object, ...) end, release = function(object) end, capacity = 0, prewarm = 0}).
int CollectionsLua::newPool(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kPoolFields});
    std::size_t capacity = 0;
    std::size_t warm = 0;
    lua::Table::readField(L, 1, "capacity", capacity);
    lua::Table::readField(L, 1, "prewarm", warm);

    const int options = 1;
    Pool& pool = lua::Userdata::emplace<Pool>(L, Pool{.capacity = capacity});
    const int self = lua_gettop(L);
    luaL_argcheck(L, lua_getfield(L, options, "create") == LUA_TFUNCTION, options, "create must be a function");
    lua::Userdata::setField(L, self, "create", -1);
    lua_pop(L, 1);
    for (const char* hook : kHooks) {
        const int type = lua_getfield(L, options, hook);
        luaL_argcheck(L, type == LUA_TFUNCTION || type == LUA_TNIL, options, "reset and release must be functions when present");
        lua::Userdata::setField(L, self, hook, -1);
        lua_pop(L, 1);
    }
    for (const char* list : kLists) {
        lua_newtable(L);
        lua::Userdata::setField(L, self, list, -1);
        lua_pop(L, 1);
    }

    // The pool takes the first stack slot, where its methods expect it.
    lua_replace(L, 1);
    lua_settop(L, 1);
    prewarm(L, pool, warm);
    return 1;
}

// Takes an idle object, or creates one, and returns it after reset(object, ...). Returns nil when the pool is at capacity.
int CollectionsLua::poolAcquire(lua_State* L) {
    Pool& pool = lua::Userdata::check<Pool>(L, 1);
    const int arguments = lua_gettop(L) - 1;
    if (pool.idle > 0) {
        lua::Userdata::pushField(L, 1, "idle");
        lua_rawgeti(L, -1, static_cast<lua_Integer>(pool.idle));
        lua_pushnil(L);
        lua_rawseti(L, -3, static_cast<lua_Integer>(pool.idle));
        lua_remove(L, -2);
        --pool.idle;
    } else if (pool.capacity > 0 && pool.active >= pool.capacity) {
        lua_pushnil(L);
        return 1;
    } else {
        create(L);
    }
    const int object = lua_gettop(L);

    const auto slot = static_cast<lua_Integer>(++pool.active);
    lua::Userdata::pushField(L, 1, "active");
    lua_pushvalue(L, object);
    lua_rawseti(L, -2, slot);
    lua::Userdata::pushField(L, 1, "slots");
    lua_pushvalue(L, object);
    lua_pushinteger(L, slot);
    lua_rawset(L, -3);
    lua_settop(L, object);

    callHook(L, "reset", object, 2, arguments);
    lua_settop(L, object);
    return 1;
}

int CollectionsLua::poolRelease(lua_State* L) {
    Pool& pool = lua::Userdata::check<Pool>(L, 1);
    luaL_checkany(L, 2);
    lua_pushboolean(L, releaseObject(L, pool, 2) ? 1 : 0);
    return 1;
}

int CollectionsLua::poolReleaseAll(lua_State* L) {
    Pool& pool = lua::Userdata::check<Pool>(L, 1);
    lua_settop(L, 1);
    pushActive(L, pool);
    const lua_Integer count = luaL_len(L, 2);
    for (lua_Integer index = 1; index <= count; ++index) {
        lua_rawgeti(L, 2, index);
        (void)releaseObject(L, pool, 3);
        lua_settop(L, 2);
    }
    return 0;
}

int CollectionsLua::poolEach(lua_State* L) {
    const Pool& pool = lua::Userdata::check<Pool>(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_settop(L, 2);
    pushActive(L, pool);
    const lua_Integer count = luaL_len(L, 3);
    for (lua_Integer index = 1; index <= count; ++index) {
        lua_pushvalue(L, 2);
        lua_rawgeti(L, 3, index);
        lua_call(L, 1, 0);
    }
    return 0;
}

int CollectionsLua::poolPrewarm(lua_State* L) {
    Pool& pool = lua::Userdata::check<Pool>(L, 1);
    const auto count = lua::Stack::read<std::size_t>(L, 2);
    lua_settop(L, 1);
    prewarm(L, pool, count);
    return 0;
}

int CollectionsLua::poolActive(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Pool>(L, 1).active);
    return 1;
}

int CollectionsLua::poolIdle(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Pool>(L, 1).idle);
    return 1;
}

int CollectionsLua::poolCapacity(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Pool>(L, 1).capacity);
    return 1;
}

int CollectionsLua::newRingBuffer(lua_State* L) {
    lua::Userdata::emplace<ValueBuffer>(L, lua::Stack::read<std::size_t>(L, 1));
    return 1;
}

// Appends a value and returns true when the oldest value made room for it.
int CollectionsLua::bufferPush(lua_State* L) {
    ValueBuffer& buffer = lua::Userdata::check<ValueBuffer>(L, 1);
    luaL_checkany(L, 2);
    lua_pushboolean(L, buffer.push(lua::Reference(L, 2)) ? 1 : 0);
    return 1;
}

int CollectionsLua::bufferPop(lua_State* L) {
    ValueBuffer& buffer = lua::Userdata::check<ValueBuffer>(L, 1);
    if (buffer.empty()) {
        lua_pushnil(L);
        return 1;
    }
    buffer.pop().push(L);
    return 1;
}

int CollectionsLua::bufferPeek(lua_State* L) {
    const ValueBuffer& buffer = lua::Userdata::check<ValueBuffer>(L, 1);
    if (buffer.empty()) {
        lua_pushnil(L);
        return 1;
    }
    buffer.front().push(L);
    return 1;
}

int CollectionsLua::bufferLast(lua_State* L) {
    const ValueBuffer& buffer = lua::Userdata::check<ValueBuffer>(L, 1);
    if (buffer.empty()) {
        lua_pushnil(L);
        return 1;
    }
    buffer.back().push(L);
    return 1;
}

// Returns the value at a position from 1 for the oldest, or nil past the newest.
int CollectionsLua::bufferGet(lua_State* L) {
    const ValueBuffer& buffer = lua::Userdata::check<ValueBuffer>(L, 1);
    const lua_Integer position = luaL_checkinteger(L, 2);
    if (position < 1 || static_cast<std::size_t>(position) > buffer.size()) {
        lua_pushnil(L);
        return 1;
    }
    buffer[static_cast<std::size_t>(position - 1)].push(L);
    return 1;
}

int CollectionsLua::bufferValues(lua_State* L) {
    const ValueBuffer& buffer = lua::Userdata::check<ValueBuffer>(L, 1);
    lua_createtable(L, static_cast<int>(buffer.size()), 0);
    lua_Integer position = 0;
    for (const lua::Reference& value : buffer) {
        value.push(L);
        lua_rawseti(L, -2, ++position);
    }
    return 1;
}

int CollectionsLua::bufferClear(lua_State* L) {
    lua::Userdata::check<ValueBuffer>(L, 1).clear();
    return 0;
}

int CollectionsLua::bufferSize(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ValueBuffer>(L, 1).size());
    return 1;
}

int CollectionsLua::bufferCapacity(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ValueBuffer>(L, 1).capacity());
    return 1;
}

int CollectionsLua::bufferFull(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ValueBuffer>(L, 1).full());
    return 1;
}

int CollectionsLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"newPool", &lua::Binding::native<&newPool>},
        {"newRingBuffer", &lua::Binding::native<&newRingBuffer>},
        {"newFloatBuffer", &lua::Binding::native<&FloatBufferLua::create>},
        {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void CollectionsLua::install(lua_State* L) {
    lua::ClassBuilder<Pool>(L).function("acquire", &lua::Binding::native<&poolAcquire>).function("release", &lua::Binding::native<&poolRelease>).function("releaseAll", &lua::Binding::native<&poolReleaseAll>).function("each", &lua::Binding::native<&poolEach>).function("prewarm", &lua::Binding::native<&poolPrewarm>).property("active", &poolActive).property("idle", &poolIdle).property("capacity", &poolCapacity).install();
    lua::ClassBuilder<ValueBuffer>(L).function("push", &lua::Binding::native<&bufferPush>).function("pop", &lua::Binding::native<&bufferPop>).function("peek", &lua::Binding::native<&bufferPeek>).function("last", &lua::Binding::native<&bufferLast>).function("get", &lua::Binding::native<&bufferGet>).function("values", &lua::Binding::native<&bufferValues>).function("clear", &lua::Binding::native<&bufferClear>).property("size", &bufferSize).property("capacity", &bufferCapacity).property("full", &bufferFull).install();
    FloatBufferLua::install(L);
    lua::Binding::preload(L, "haylen.collections", &open);
}

} // namespace haylen::core
