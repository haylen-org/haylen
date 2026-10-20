#include "content/LuaPackSource.hpp"

#include <new>
#include <utility>

#include "haylen/lua/Binding.hpp"
#include "haylen/lua/Promise.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/Userdata.hpp"
#include "lua/StackScope.hpp"
#include "lua/Task.hpp"

namespace haylen::content {

LuaPackSource::LuaPackSource(lua_State* L, int functionIndex, int ownerIndex) : main(lua::Runtime::getMainThread(L)), function(std::make_shared<lua::Owners::Function>(L, functionIndex, ownerIndex)), owner(ownerIndex != 0 ? std::make_shared<lua::WeakReference>(L, ownerIndex) : nullptr) {}

void LuaPackSource::Answer::deliver(std::optional<std::filesystem::path> folder, std::optional<Error> failure) {
    if (!done) {
        return;
    }
    const Delivery finish = std::exchange(done, nullptr);
    finish(std::move(folder), std::move(failure));
}

void LuaPackSource::acquire(const std::string& pack, std::function<void(const Progress&)> progress, Delivery done) {
    auto answer = std::make_shared<Answer>(Answer{.done = std::move(done)});
    lua_State* L = main;
    const lua::StackScope scope(L);

    // A source whose owner is gone delivers nothing, and its function never runs again.
    int ownerIndex = 0;
    if (owner) {
        if (!owner->push(L)) {
            answer->deliver(std::nullopt, std::nullopt);
            return;
        }
        ownerIndex = lua_gettop(L);
    }
    new (lua_newuserdatauv(L, sizeof(std::shared_ptr<Answer>), 0)) std::shared_ptr<Answer>(answer);
    if (lua::Userdata::newMetatable(L, kRequestType)) {
        lua_pushcfunction(L, &collect<std::shared_ptr<Answer>>);
        lua_setfield(L, -2, "__gc");
    }
    lua_setmetatable(L, -2);
    lua_pushcclosure(L, &run, 1);
    const int runner = lua_gettop(L);
    if (!function->push(L)) {
        answer->deliver(std::nullopt, std::nullopt);
        return;
    }
    lua_pushlstring(L, pack.data(), pack.size());
    new (lua_newuserdatauv(L, sizeof(std::function<void(const Progress&)>), 0)) std::function<void(const Progress&)>(std::move(progress));
    if (lua::Userdata::newMetatable(L, kProgressType)) {
        lua_pushcfunction(L, &collect<std::function<void(const Progress&)>>);
        lua_setfield(L, -2, "__gc");
    }
    lua_setmetatable(L, -2);
    lua_pushcclosure(L, &reportProgress, 1);

    // clang-format off
    lua::Task::start(L, runner, 3, ownerIndex, [answer, pack](const std::optional<lua::Error>& error) {
        if (error) {
            answer->deliver(std::nullopt, Error(Error::Code::StoreAssetUnavailable, "The source of packs failed to deliver the pack \"" + pack + "\". " + error->getMessage()));
        }
    });
    // clang-format on
}

int LuaPackSource::run(lua_State* L) {
    lua_callk(L, 2, 1, kCalled, &continueRun);
    return continueRun(L, LUA_OK, kCalled);
}

// Runs once the function returned, and again once a promise it returned settled, with its value and the reason it failed.
int LuaPackSource::continueRun(lua_State* L, int, lua_KContext context) {
    if (context == kCalled && lua::Promise::isPromise(L, -1)) {
        lua_getfield(L, -1, "await");
        lua_insert(L, -2);
        lua_callk(L, 1, 2, kAwaited, &continueRun);
        return continueRun(L, LUA_OK, kAwaited);
    }
    if (context == kAwaited) {
        if (!lua_isnil(L, -1)) {
            return lua_error(L);
        }
        lua_pop(L, 1);
    }
    if (!lua_isnil(L, -1) && lua_type(L, -1) != LUA_TSTRING) {
        return luaL_error(L, "The source of packs must return the folder of the pack, nil or a promise of either.");
    }
    // clang-format off
    return lua::Binding::guarded(L, [L] {
        Answer& answer = **static_cast<std::shared_ptr<Answer>*>(lua_touserdata(L, lua_upvalueindex(1)));
        if (lua_isnil(L, -1)) {
            answer.deliver(std::nullopt, std::nullopt);
        } else {
            answer.deliver(std::filesystem::path(lua_tostring(L, -1)), std::nullopt);
        }
        return 0;
    });
    // clang-format on
}

int LuaPackSource::reportProgress(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    // clang-format off
    return lua::Binding::guarded(L, [L] {
        Progress value;
        lua::Table::readField(L, 1, "receivedBytes", value.receivedBytes);
        lua::Table::readField(L, 1, "totalBytes", value.totalBytes);
        (*static_cast<std::function<void(const Progress&)>*>(lua_touserdata(L, lua_upvalueindex(1))))(value);
        return 0;
    });
    // clang-format on
}

template <typename T> int LuaPackSource::collect(lua_State* L) {
    static_cast<T*>(lua_touserdata(L, 1))->~T();
    lua_pushnil(L);
    lua_setmetatable(L, 1);
    return 0;
}

} // namespace haylen::content
