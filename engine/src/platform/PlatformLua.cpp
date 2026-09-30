#include "platform/PlatformLua.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/Bytes.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Reference.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "platform/StreamsLua.hpp"
#include "varn/async/Promise.h"

namespace haylen::platform {

// Parameters that Lua leaves out are an empty object, and strings marked with platform.bytes become byte buffers.
Bridge::Payload PlatformLua::readPayload(lua_State* L, int index) {
    Bridge::Payload payload;
    payload.json = lua_isnoneornil(L, index) ? core::Json::object() : lua::JsonConverter::read(L, index, payload.buffers);
    return payload;
}

void PlatformLua::pushPayload(lua_State* L, const Bridge::Payload& payload) {
    lua::JsonConverter::push(L, payload.json, payload.buffers);
}

void PlatformLua::pushCall(lua_State* L, const std::string& method, int paramsIndex, int optionsIndex) {
    core::Engine& owner = lua::Runtime::getEngine(L);
    const Bridge::Payload params = readPayload(L, paramsIndex);
    std::optional<std::chrono::steady_clock::duration> timeout;
    if (!lua_isnoneornil(L, optionsIndex)) {
        luaL_checktype(L, optionsIndex, LUA_TTABLE);
        lua::Table::checkFields(L, optionsIndex, {kCallOptions});
        double seconds = 0.0;
        lua::Table::readField(L, optionsIndex, "timeout", seconds);
        if (!std::isfinite(seconds) || seconds <= 0.0) {
            throw std::invalid_argument("The timeout of a platform call is a positive number of seconds.");
        }
        timeout = std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(seconds));
    }

    auto pending = std::make_shared<Call>();
    pending->promise = std::make_shared<varn::async::Promise>(owner.getScriptRuntime());
    pending->cancel = &cancelBridgeCall;
    pending->id = owner.getPlatform().call(method, params, settle(pending), timeout);
    lua::Userdata::emplace<Call>(L, std::move(pending));
}

Bridge::Callback PlatformLua::settle(std::shared_ptr<Call> pending) {
    // clang-format off
    return [pending = std::move(pending)](Bridge::Result result) {
        if (!result.ok) {
            pending->error = std::move(result.error);
            pending->promise->reject(pending->error->message);
            return;
        }
        pending->promise->resolveCustom([value = std::move(result.value)](lua_State* state) { pushPayload(state, value); });
    };
    // clang-format on
}

void PlatformLua::pushConnection(lua_State* L, const std::string& event, int listenerIndex) {
    core::Engine& owner = lua::Runtime::getEngine(L);
    luaL_checktype(L, listenerIndex, LUA_TFUNCTION);
    auto function = std::make_shared<lua::Reference>(L, listenerIndex);

    // clang-format off
    core::Connection connection = owner.getPlatform().on(event, [function](const Bridge::Payload& payload) {
        lua_State* main = function->getState();
        lua::Runtime::runReporting(main, [&] {
            function->push(main);
            pushPayload(main, payload);
            lua::Runtime::protectedCall(main, 1, 0);
        });
    });
    // clang-format on

    lua::Userdata::emplace<core::Connection>(L, std::move(connection));
}

// Calls a native method with call(method, params, {timeout = seconds}) and returns the call, which Lua awaits and may cancel.
int PlatformLua::call(lua_State* L) {
    pushCall(L, lua::Stack::read<std::string>(L, 1), 2, 3);
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

bool PlatformLua::cancelBridgeCall(lua_State* L, std::uint64_t id) {
    return lua::Runtime::getEngine(L).getPlatform().cancel(id);
}

bool PlatformLua::cancelScreen(lua_State* L, std::uint64_t id) {
    return lua::Runtime::getEngine(L).getScreens().cancel(id);
}

// Gives up the call, which fails with the code cancelled, and returns whether it was still pending.
int PlatformLua::cancel(lua_State* L) {
    const std::shared_ptr<Call>& pending = lua::Userdata::checkShared<Call>(L, 1);
    lua_pushboolean(L, pending->cancel(L, pending->id) ? 1 : 0);
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
    std::vector<std::vector<std::byte>> buffers;
    const std::string result = lua::JsonConverter::read(L, 3, buffers).dump();
    lua::Runtime::getEngine(L).getPlatform().resolve(id, ok, result, std::move(buffers));
    return 0;
}

// Sends an event with emit(event, payload, {retain = true, batched = true}) the way native code does, so platform.on listeners receive it at the start of the next frame.
int PlatformLua::emit(lua_State* L) {
    const std::string event = lua::Stack::read<std::string>(L, 1);
    std::vector<std::vector<std::byte>> buffers;
    const std::string payload = lua::JsonConverter::read(L, 2, buffers).dump();
    Bridge::EmitOptions options;
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        lua::Table::checkFields(L, 3, {kEmitOptions});
        lua::Table::readField(L, 3, "retain", options.retain);
        lua::Table::readField(L, 3, "batched", options.batched);
    }
    lua::Runtime::getEngine(L).getPlatform().emit(event, payload, std::move(buffers), options);
    return 0;
}

// Marks a string with bytes(data) to cross the bridge as a byte buffer, since plain strings cross as text.
int PlatformLua::bytes(lua_State* L) {
    const std::string_view data = lua::Stack::read<std::string_view>(L, 1);
    const auto* first = reinterpret_cast<const std::byte*>(data.data());
    lua::Userdata::emplace<lua::Bytes>(L, lua::Bytes{.data = {first, first + data.size()}});
    return 1;
}

int PlatformLua::getBytesSize(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<lua::Bytes>(L, 1).data.size());
    return 1;
}

int PlatformLua::pendingCallCount(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getPlatform().getPendingCallCount());
    return 1;
}

// Subscribes to native events with on(name, function(payload)) and returns a connection with a disconnect method.
int PlatformLua::on(lua_State* L) {
    pushConnection(L, lua::Stack::read<std::string>(L, 1), 2);
    return 1;
}

// Calls a method with send(method, params) when nothing needs its answer, which creates no call and drops the answer.
int PlatformLua::send(lua_State* L) {
    const std::string method = lua::Stack::read<std::string>(L, 1);
    lua::Runtime::getEngine(L).getPlatform().send(method, readPayload(L, 2));
    return 0;
}

// Implements a platform method in Lua with registerHandler(method, function(params) return result end), which is useful on desktop and in tests.
int PlatformLua::registerHandler(lua_State* L) {
    core::Engine& owner = lua::Runtime::getEngine(L);
    const std::string method = lua::Stack::read<std::string>(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    auto function = std::make_shared<lua::Reference>(L, 2);

    // clang-format off
    owner.getPlatform().registerHandler(method, [function](const Bridge::Payload& params, Bridge::Reply reply) {
        lua_State* state = function->getState();
        const int top = lua_gettop(state);
        Bridge::Result result;
        try {
            function->push(state);
            pushPayload(state, params);
            lua::Runtime::protectedCall(state, 1, 1);
            result = {.ok = true};
            result.value.json = lua::JsonConverter::read(state, -1, result.value.buffers);
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

// Lists the plugins of app.json as tables with id, version and native.
int PlatformLua::plugins(lua_State* L) {
    const std::vector<AppPlugin> all = lua::Runtime::getEngine(L).getAppPlugins();
    lua_createtable(L, static_cast<int>(all.size()), 0);
    for (std::size_t index = 0; index < all.size(); ++index) {
        lua_createtable(L, 0, 3);
        lua::Stack::push(L, all[index].id);
        lua_setfield(L, -2, "id");
        lua::Stack::push(L, all[index].version);
        lua_setfield(L, -2, "version");
        lua::Stack::push(L, all[index].native);
        lua_setfield(L, -2, "native");
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    return 1;
}

// Returns the handle of a plugin of app.json with plugin(id), which the Lua modules of the plugin use to reach its native part.
int PlatformLua::plugin(lua_State* L) {
    const std::string id = lua::Stack::read<std::string>(L, 1);
    for (AppPlugin& found : lua::Runtime::getEngine(L).getAppPlugins()) {
        if (found.id == id) {
            lua::Userdata::emplace<AppPlugin>(L, std::move(found));
            return 1;
        }
    }
    throw std::invalid_argument("The plugin " + id + " is not among the plugins of app.json.");
}

int PlatformLua::getPluginId(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<AppPlugin>(L, 1).id);
    return 1;
}

int PlatformLua::getPluginVersion(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<AppPlugin>(L, 1).version);
    return 1;
}

int PlatformLua::getPluginConfig(lua_State* L) {
    lua::JsonConverter::push(L, lua::Userdata::check<AppPlugin>(L, 1).config);
    return 1;
}

// A native library may declare the native part of the plugin after the handle was made, so the handle asks every time.
int PlatformLua::isPluginNative(lua_State* L) {
    const std::vector<std::string> loaded = lua::Runtime::getEngine(L).getNativePlugins();
    lua::Stack::push(L, std::ranges::binary_search(loaded, lua::Userdata::check<AppPlugin>(L, 1).id));
    return 1;
}

// Plugins name their methods and events after their id, so the handle puts the id in front of the name that its method receives.
std::string PlatformLua::getPluginName(lua_State* L) {
    const std::string& id = lua::Userdata::check<AppPlugin>(L, 1).id;
    const std::string name = lua::Stack::read<std::string>(L, 2);
    if (name.empty()) {
        throw std::invalid_argument("A method or event of the plugin " + id + " needs a name.");
    }
    return id + "." + name;
}

int PlatformLua::callPlugin(lua_State* L) {
    pushCall(L, getPluginName(L), 3, 4);
    return 1;
}

int PlatformLua::sendToPlugin(lua_State* L) {
    const std::string method = getPluginName(L);
    lua::Runtime::getEngine(L).getPlatform().send(method, readPayload(L, 3));
    return 0;
}

int PlatformLua::onPlugin(lua_State* L) {
    pushConnection(L, getPluginName(L), 3);
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

// Returns the video stream name of the plugin with handle:videoStream(name), or nil while its native part has not opened it.
int PlatformLua::videoStreamOfPlugin(lua_State* L) {
    StreamsLua::pushVideoStream(L, lua::Userdata::check<AppPlugin>(L, 1).id, lua::Stack::read<std::string>(L, 2));
    return 1;
}

int PlatformLua::audioStreamOfPlugin(lua_State* L) {
    StreamsLua::pushAudioStream(L, lua::Userdata::check<AppPlugin>(L, 1).id, lua::Stack::read<std::string>(L, 2));
    return 1;
}

// Opens a screen of the plugin with handle:openScreen(name, params, {state = value, opaque = true, timeout = seconds}) and returns its call, which Lua awaits and may cancel.
int PlatformLua::openScreenOfPlugin(lua_State* L) {
    core::Engine& owner = lua::Runtime::getEngine(L);
    const std::string& id = lua::Userdata::check<AppPlugin>(L, 1).id;
    std::string screen = lua::Stack::read<std::string>(L, 2);
    if (screen.empty()) {
        throw std::invalid_argument("A screen of the plugin " + id + " needs a name.");
    }
    Bridge::Payload params = readPayload(L, 3);
    Screens::Options options = readScreenOptions(L, 4);

    auto pending = std::make_shared<Call>();
    pending->promise = std::make_shared<varn::async::Promise>(owner.getScriptRuntime());
    pending->cancel = &cancelScreen;
    pending->id = owner.getScreens().open(id, std::move(screen), std::move(params), std::move(options), settle(pending));
    lua::Userdata::emplace<Call>(L, std::move(pending));
    return 1;
}

// The state goes where the platform keeps it across the end of the process, such as the session storage of a page, which holds text, so it takes no bytes.
Screens::Options PlatformLua::readScreenOptions(lua_State* L, int index) {
    Screens::Options options;
    if (lua_isnoneornil(L, index)) {
        return options;
    }
    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kScreenOptions});
    lua::Table::readField(L, index, "opaque", options.opaque);

    std::optional<double> seconds;
    lua::Table::readField(L, index, "timeout", seconds);
    if (seconds) {
        if (!std::isfinite(*seconds) || *seconds <= 0.0) {
            throw std::invalid_argument("The timeout of a screen is a positive number of seconds.");
        }
        options.timeout = std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(*seconds));
    }

    lua_getfield(L, index, "state");
    std::vector<std::vector<std::byte>> buffers;
    options.state = lua_isnil(L, -1) ? core::Json(nullptr) : lua::JsonConverter::read(L, -1, buffers);
    lua_pop(L, 1);
    if (!buffers.empty()) {
        throw std::invalid_argument("The state of a screen is JSON without bytes.");
    }
    return options;
}

int PlatformLua::screenShowing(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getScreens().isShowing());
    return 1;
}

int PlatformLua::open(lua_State* L) {
    lua::ClassBuilder<AppPlugin>(L).property("id", &getPluginId).property("version", &getPluginVersion).property("config", &getPluginConfig).property("native", &lua::Binding::native<&isPluginNative>).function("call", &lua::Binding::native<&callPlugin>).function("send", &lua::Binding::native<&sendToPlugin>).function("on", &lua::Binding::native<&onPlugin>).function("videoStream", &lua::Binding::native<&videoStreamOfPlugin>).function("audioStream", &lua::Binding::native<&audioStreamOfPlugin>).function("openScreen", &lua::Binding::native<&openScreenOfPlugin>).install();

    const luaL_Reg functions[] = {
        {"call", &lua::Binding::native<&call>}, {"on", &lua::Binding::native<&on>}, {"send", &lua::Binding::native<&send>}, {"registerHandler", &lua::Binding::native<&registerHandler>}, {"hasHandler", &hasHandler}, {"resolve", &lua::Binding::native<&resolve>}, {"emit", &lua::Binding::native<&emit>}, {"bytes", &lua::Binding::native<&bytes>}, {"pendingCallCount", &pendingCallCount}, {"screenShowing", &screenShowing}, {"plugins", &lua::Binding::native<&plugins>}, {"plugin", &lua::Binding::native<&plugin>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

// Calls and their errors exist before any module is required, because haylen.dialogs returns calls too.
void PlatformLua::install(lua_State* L) {
    luaL_newmetatable(L, kErrorType);
    lua_pushcfunction(L, &errorToString);
    lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, &concatError);
    lua_setfield(L, -2, "__concat");
    lua_pop(L, 1);
    lua::ClassBuilder<Call>(L).function("await", &await).function("cancel", &cancel).property("id", &getId).property("done", &isDone).property("promise", &getPromise).install();
    lua::ClassBuilder<lua::Bytes>(L).property("size", &getBytesSize).install();
    StreamsLua::install(L);
    lua::Binding::preload(L, "haylen.platform", &open);
}

} // namespace haylen::platform
