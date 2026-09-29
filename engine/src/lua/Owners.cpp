#include "lua/Owners.hpp"

#include <memory>
#include <new>
#include <utility>

#include "haylen/core/Engine.hpp"
#include "haylen/core/FrameQueue.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Userdata.hpp"
#include "lua/Task.hpp"

namespace haylen::lua {

void Owners::pushWeakTable(lua_State* L, const char* name, const char* mode) {
    if (lua_getfield(L, LUA_REGISTRYINDEX, name) == LUA_TTABLE) {
        return;
    }
    lua_pop(L, 1);
    lua_newtable(L);
    lua_createtable(L, 0, 1);
    lua_pushstring(L, mode);
    lua_setfield(L, -2, "__mode");
    lua_setmetatable(L, -2);
    lua_pushvalue(L, -1);
    lua_setfield(L, LUA_REGISTRYINDEX, name);
}

void Owners::checkOwner(lua_State* L, int index) {
    const int type = lua_type(L, index);
    if (type != LUA_TTABLE && type != LUA_TUSERDATA) {
        luaL_error(L, "An owner must be a table or a userdata, not %s.", luaL_typename(L, index));
    }
}

int Owners::collectScope(lua_State* L) {
    auto* scope = static_cast<Scope*>(lua_touserdata(L, 1));

    // The collector may run in the middle of any Lua call, so what the owner held ends at the end of the frame instead of here, where kill callbacks and to-be-closed variables could run Lua inside the collector.
    if ((!scope->connections.empty() || !scope->links.empty() || !scope->tasks.empty()) && scope->queue != nullptr) {
        auto connections = std::make_shared<core::ConnectionScope>(std::move(scope->connections));
        // clang-format off
        scope->queue->post([connections, tasks = std::move(scope->tasks), links = std::move(scope->links)]() mutable {
            endHeld(std::move(tasks), std::move(links), *connections);
        });
        // clang-format on
    }
    scope->~Scope();
    lua_pushnil(L);
    lua_setmetatable(L, 1);
    return 0;
}

Owners::Scope& Owners::pushScope(lua_State* L, int owner) {
    const int ownerIndex = lua_absindex(L, owner);
    pushWeakTable(L, kScopes, "k");
    lua_pushvalue(L, ownerIndex);
    if (lua_rawget(L, -2) == LUA_TUSERDATA) {
        lua_remove(L, -2);
        return *static_cast<Scope*>(lua_touserdata(L, -1));
    }
    lua_pop(L, 1);

    if (Userdata::newMetatable(L, kScopeType)) {
        lua_pushcfunction(L, &collectScope);
        lua_setfield(L, -2, "__gc");
    }
    lua_pop(L, 1);

    // The scope keeps the functions of the owner's listeners in its user value, reachable only through the owner.
    auto* scope = new (lua_newuserdatauv(L, sizeof(Scope), 1)) Scope{.queue = &Runtime::getEngine(L).getFrameQueue()};
    luaL_setmetatable(L, kScopeType);
    lua_newtable(L);
    lua_setiuservalue(L, -2, 1);
    lua_pushvalue(L, ownerIndex);
    lua_pushvalue(L, -2);
    lua_rawset(L, -4);
    lua_remove(L, -2);
    return *scope;
}

Owners::Scope* Owners::findScope(lua_State* L, int owner) {
    const int ownerIndex = lua_absindex(L, owner);
    pushWeakTable(L, kScopes, "k");
    lua_pushvalue(L, ownerIndex);
    Scope* scope = lua_rawget(L, -2) == LUA_TUSERDATA ? static_cast<Scope*>(lua_touserdata(L, -1)) : nullptr;
    lua_pop(L, 2);
    return scope;
}

std::weak_ptr<const void> Owners::getLifetime(lua_State* L, int owner) {
    checkOwner(L, owner);
    const std::weak_ptr<const void> lifetime = pushScope(L, owner).lifetime;
    lua_pop(L, 1);
    return lifetime;
}

void Owners::add(lua_State* L, int owner, core::Connection connection) {
    checkOwner(L, owner);
    pushScope(L, owner).connections.add(std::move(connection));
    lua_pop(L, 1);
}

// Links and tasks that ended on their own leave the lists whenever something new joins them, so an owner that lives for the whole app stays small.
void Owners::add(lua_State* L, int owner, std::shared_ptr<core::Connection::Link> link) {
    checkOwner(L, owner);
    Scope& scope = pushScope(L, owner);
    lua_pop(L, 1);
    std::erase_if(scope.links, [](const std::shared_ptr<core::Connection::Link>& held) { return !held->isConnected(); });
    scope.links.push_back(std::move(link));
}

void Owners::addTask(lua_State* L, int owner, const std::shared_ptr<Task>& task) {
    checkOwner(L, owner);
    Scope& scope = pushScope(L, owner);
    lua_pop(L, 1);
    std::erase_if(scope.tasks, [](const std::weak_ptr<Task>& held) { return held.expired() || !held.lock()->isRunning(); });
    scope.tasks.push_back(task);
}

void Owners::release(lua_State* L, int owner) {
    if (Scope* scope = findScope(L, owner)) {
        endHeld(std::exchange(scope->tasks, {}), std::exchange(scope->links, {}), scope->connections);
    }
}

void Owners::endHeld(std::vector<std::weak_ptr<Task>> tasks, std::vector<std::shared_ptr<core::Connection::Link>> links, core::ConnectionScope& connections) {
    for (const std::weak_ptr<Task>& held : tasks) {
        if (const std::shared_ptr<Task> task = held.lock()) {
            task->cancel();
        }
    }
    for (const std::shared_ptr<core::Connection::Link>& link : links) {
        link->disconnect();
    }
    connections.clear();
}

Owners::Function::Function(lua_State* L, int function, int owner) : state(Runtime::getMainThread(L)) {
    luaL_checktype(L, function, LUA_TFUNCTION);
    if (owner == 0) {
        strong = Reference(L, function);
        return;
    }

    checkOwner(L, owner);
    const int functionIndex = lua_absindex(L, function);
    pushScope(L, owner);
    const int scope = lua_gettop(L);

    // Keys come from a counter in slot 0 of the weak table, which numbers never leave.
    pushWeakTable(L, kFunctions, "v");
    const int functions = lua_gettop(L);
    lua_rawgeti(L, functions, 0);
    key = lua_tointeger(L, -1) + 1;
    lua_pop(L, 1);
    lua_pushinteger(L, key);
    lua_rawseti(L, functions, 0);
    lua_pushvalue(L, functionIndex);
    lua_rawseti(L, functions, key);

    lua_getiuservalue(L, scope, 1);
    lua_pushvalue(L, functionIndex);
    lua_rawseti(L, -2, key);
    lua_pop(L, 1);

    pushWeakTable(L, kFunctionScopes, "v");
    lua_pushvalue(L, scope);
    lua_rawseti(L, -2, key);
    lua_settop(L, scope - 1);
}

Owners::Function::~Function() {
    if (key == 0) {
        return;
    }
    lua_State* L = state;
    pushWeakTable(L, kFunctions, "v");
    lua_pushnil(L);
    lua_rawseti(L, -2, key);
    lua_pop(L, 1);

    pushWeakTable(L, kFunctionScopes, "v");
    if (lua_rawgeti(L, -1, key) == LUA_TUSERDATA) {
        lua_getiuservalue(L, -1, 1);
        lua_pushnil(L);
        lua_rawseti(L, -2, key);
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    lua_pushnil(L);
    lua_rawseti(L, -2, key);
    lua_pop(L, 1);
}

bool Owners::Function::push(lua_State* L) const {
    if (key == 0) {
        strong.push(L);
        return true;
    }
    pushWeakTable(L, kFunctions, "v");
    const bool present = lua_rawgeti(L, -1, key) == LUA_TFUNCTION;
    lua_remove(L, -2);
    if (!present) {
        lua_pop(L, 1);
    }
    return present;
}

} // namespace haylen::lua
