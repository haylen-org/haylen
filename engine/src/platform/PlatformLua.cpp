#include "platform/PlatformLua.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Reference.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "varn/async/Promise.h"

namespace haylen::platform {

// Calls a native method with call(method, params, {timeout = seconds}) and returns the call, which Lua awaits and may cancel.
int PlatformLua::call(lua_State* L) {
    core::Engine& owner = lua::Runtime::getEngine(L);
    const std::string method = lua::Stack::read<std::string>(L, 1);
    const core::Json params = lua_isnoneornil(L, 2) ? core::Json::object() : lua::JsonConverter::read(L, 2);
    std::optional<std::chrono::steady_clock::duration> timeout;
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        lua::Table::checkFields(L, 3, {kCallOptions});
        double seconds = 0.0;
        lua::Table::readField(L, 3, "timeout", seconds);
        if (!std::isfinite(seconds) || seconds <= 0.0) {
            throw std::invalid_argument("The timeout of a platform call is a positive number of seconds.");
        }
        timeout = std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(seconds));
    }

    auto pending = std::make_shared<Call>();
    pending->promise = std::make_shared<varn::async::Promise>(owner.getScriptRuntime());
    // clang-format off
    pending->id = owner.getPlatform().call(method, params, [pending](Bridge::Result result) {
        if (!result.ok) {
            pending->error = std::move(result.error);
            pending->promise->reject(pending->error->message);
            return;
        }
        pending->promise->resolveCustom([value = std::move(result.value)](lua_State* state) { lua::JsonConverter::push(state, value); });
    }, timeout);
    // clang-format on

    lua::Userdata::emplace<Call>(L, std::move(pending));
    return 1;
}

// Waits on the promise of the call and turns its failure into the error table. The promise may suspend the coroutine, so the rest runs as a continuation.
int PlatformLua::await(lua_State* L) {
    const std::shared_ptr<Call>& pending = lua::Userdata::checkShared<Call>(L, 1);
    lua_settop(L, 1);
    varn::async::Promise::push(L, pending->promise);
    lua_getfield(L, -1, "await");
    lua_insert(L, -2);
    lua_callk(L, 1, LUA_MULTRET, 0, &finishAwait);
    return finishAwait(L, LUA_OK, 0);
}

int PlatformLua::finishAwait(lua_State* L, int, lua_KContext) {
    const int results = lua_gettop(L) - 1;
    const std::shared_ptr<Call>& pending = lua::Userdata::checkShared<Call>(L, 1);
    if (results < 2 || !lua_isnil(L, 2) || !pending->error) {
        return results;
    }
    lua_settop(L, 1);
    lua_pushnil(L);
    pushError(L, *pending->error);
    return 2;
}

// Gives up the call, which fails with the code cancelled, and returns whether it was still pending.
int PlatformLua::cancel(lua_State* L) {
    const std::shared_ptr<Call>& pending = lua::Userdata::checkShared<Call>(L, 1);
    lua_pushboolean(L, lua::Runtime::getEngine(L).getPlatform().cancel(pending->id) ? 1 : 0);
    return 1;
}

int PlatformLua::getId(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Call>(L, 1).id);
    return 1;
}

int PlatformLua::isDone(lua_State* L) {
    lua_pushboolean(L, lua::Userdata::check<Call>(L, 1).promise->state() != varn::async::Promise::State::Pending ? 1 : 0);
    return 1;
}

// The promise of Varn behind the call, for the combinators of async such as async.all, where a failure is only its message.
int PlatformLua::getPromise(lua_State* L) {
    varn::async::Promise::push(L, lua::Userdata::check<Call>(L, 1).promise);
    return 1;
}

// Answers a pending call with resolve(id, ok, result) the way native code does, where a failure passes a message or a table with message, code and data.
int PlatformLua::resolve(lua_State* L) {
    const auto id = lua::Stack::read<std::uint64_t>(L, 1);
    const bool ok = lua::Stack::read<bool>(L, 2);
    lua::Runtime::getEngine(L).getPlatform().resolve(id, ok, lua::JsonConverter::read(L, 3).dump());
    return 0;
}

// Sends an event with emit(event, payload) the way native code does, so platform.on listeners receive it at the start of the next frame.
int PlatformLua::emit(lua_State* L) {
    const std::string event = lua::Stack::read<std::string>(L, 1);
    lua::Runtime::getEngine(L).getPlatform().emit(event, lua::JsonConverter::read(L, 2).dump());
    return 0;
}

int PlatformLua::pendingCalls(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getPlatform().getPendingCallCount());
    return 1;
}

// Subscribes to native events with on(name, function(payload)) and returns a connection with a disconnect method.
int PlatformLua::on(lua_State* L) {
    core::Engine& owner = lua::Runtime::getEngine(L);
    const std::string name = lua::Stack::read<std::string>(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    auto function = std::make_shared<lua::Reference>(L, 2);

    // clang-format off
    core::Connection connection = owner.getPlatform().on(name, [function](const core::Json& payload) {
        lua_State* main = function->getState();
        lua::Runtime::runReporting(main, [&] {
            function->push(main);
            lua::JsonConverter::push(main, payload);
            lua::Runtime::protectedCall(main, 1, 0);
        });
    });
    // clang-format on

    lua::Userdata::emplace<core::Connection>(L, std::move(connection));
    return 1;
}

// Implements a platform method in Lua with register(method, function(params) return result end), which is useful on desktop and in tests.
int PlatformLua::registerHandler(lua_State* L) {
    core::Engine& owner = lua::Runtime::getEngine(L);
    const std::string method = lua::Stack::read<std::string>(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    auto function = std::make_shared<lua::Reference>(L, 2);

    // clang-format off
    owner.getPlatform().registerHandler(method, [function](const core::Json& params, Bridge::Reply reply) {
        lua_State* state = function->getState();
        const int top = lua_gettop(state);
        Bridge::Result result;
        try {
            function->push(state);
            lua::JsonConverter::push(state, params);
            lua::Runtime::protectedCall(state, 1, 1);
            result = {.ok = true, .value = lua::JsonConverter::read(state, -1)};
        } catch (const std::exception& exception) {
            result = {.error = {.message = exception.what()}};
        }
        lua_settop(state, top);
        reply(std::move(result));
    });
    // clang-format on
    return 0;
}

int PlatformLua::hasHandler(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getPlatform().hasHandler(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

void PlatformLua::pushError(lua_State* L, const Bridge::Error& error) {
    lua_createtable(L, 0, 3);
    lua::Stack::push(L, error.message);
    lua_setfield(L, -2, "message");
    lua::JsonConverter::push(L, error.code);
    lua_setfield(L, -2, "code");
    lua::JsonConverter::push(L, error.data);
    lua_setfield(L, -2, "data");
    luaL_setmetatable(L, kErrorType);
}

int PlatformLua::errorToString(lua_State* L) {
    lua_getfield(L, 1, "message");
    return 1;
}

// Concatenation reads an error as its message, so code written for message strings keeps working.
int PlatformLua::concatError(lua_State* L) {
    for (int index = 1; index <= 2; ++index) {
        const int name = lua_istable(L, index) ? luaL_getmetafield(L, index, "__name") : LUA_TNIL;
        if (name != LUA_TNIL) {
            const bool error = name == LUA_TSTRING && std::string_view(lua_tostring(L, -1)) == kErrorType;
            lua_pop(L, 1);
            if (error) {
                lua_getfield(L, index, "message");
                continue;
            }
        }
        luaL_tolstring(L, index, nullptr);
    }
    lua_concat(L, 2);
    return 1;
}

int PlatformLua::open(lua_State* L) {
    luaL_newmetatable(L, kErrorType);
    lua_pushcfunction(L, &errorToString);
    lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, &concatError);
    lua_setfield(L, -2, "__concat");
    lua_pop(L, 1);

    lua::ClassBuilder<Call>(L).function("await", &await).function("cancel", &cancel).property("id", &getId).property("done", &isDone).property("promise", &getPromise).install();

    const luaL_Reg functions[] = {
        {"call", &lua::Binding::native<&call>}, {"on", &lua::Binding::native<&on>}, {"register", &lua::Binding::native<&registerHandler>}, {"hasHandler", &hasHandler}, {"resolve", &lua::Binding::native<&resolve>}, {"emit", &lua::Binding::native<&emit>}, {"pendingCalls", &pendingCalls}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void PlatformLua::install(lua_State* L) {
    lua::Binding::preload(L, "haylen.platform", &open);
}

} // namespace haylen::platform
