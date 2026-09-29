#include "platform/PlatformLua.hpp"

#include <cstdint>
#include <memory>
#include <string>

#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Reference.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/platform/Bridge.hpp"
#include "varn/async/Promise.h"

namespace haylen::platform {

// Calls a native method with call(method, params) and returns a promise for its JSON result and the id of the call.
int PlatformLua::call(lua_State* L) {
    core::Engine& owner = lua::Runtime::getEngine(L);
    const std::string method = lua::Stack::read<std::string>(L, 1);
    const core::Json params = lua_isnoneornil(L, 2) ? core::Json::object() : lua::JsonConverter::read(L, 2);
    auto promise = std::make_shared<varn::async::Promise>(owner.getScriptRuntime());

    // clang-format off
    const std::uint64_t id = owner.getPlatform().call(method, params, [promise](Bridge::Result result) {
        if (!result.ok) {
            promise->reject(result.error);
            return;
        }
        promise->resolveCustom([value = std::move(result.value)](lua_State* state) { lua::JsonConverter::push(state, value); });
    });
    // clang-format on

    varn::async::Promise::push(L, promise);
    lua::Stack::push(L, id);
    return 2;
}

// Answers a pending call with resolve(id, ok, result) the way native code does, where a failure passes a message or a table with a message.
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
            result = {.ok = false, .error = exception.what()};
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

int PlatformLua::open(lua_State* L) {
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
