#include "net/NetLua.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Reference.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/plugins/NetPlugin.hpp"

namespace haylen::net {

// Reads reconnect = true for the default backoff or a table that changes some of its settings.
WebSocket::Reconnect NetLua::readReconnect(lua_State* L, int index) {
    WebSocket::Reconnect reconnect;
    if (lua_isboolean(L, index)) {
        reconnect.enabled = lua_toboolean(L, index) != 0;
        return reconnect;
    }
    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kReconnectOptions});
    reconnect.enabled = true;
    lua::Table::readField(L, index, "initialDelay", reconnect.initialDelay);
    lua::Table::readField(L, index, "maxDelay", reconnect.maxDelay);
    lua::Table::readField(L, index, "multiplier", reconnect.multiplier);
    lua::Table::readField(L, index, "jitter", reconnect.jitter);
    lua::Table::readField(L, index, "maxAttempts", reconnect.maxAttempts);
    return reconnect;
}

// Opens a WebSocket with websocket(url, {protocols = {...}, reconnect = true or {...}}). The socket keeps delivering events until it closes, even when the app no longer holds it.
int NetLua::websocket(lua_State* L) {
    std::string url = lua::Stack::read<std::string>(L, 1);
    WebSocket::Options options;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kSocketOptions});
        lua::Table::readField(L, 2, "protocols", options.protocols);
        if (lua_getfield(L, 2, "reconnect") != LUA_TNIL) {
            options.reconnect = readReconnect(L, lua_gettop(L));
        }
        lua_pop(L, 1);
    }
    lua::Userdata::emplace<WebSocket>(L, lua::Runtime::getEngine(L).getPlugin<plugins::NetPlugin>().connectWebSocket(std::move(url), std::move(options)));
    return 1;
}

WebSocket& NetLua::check(lua_State* L) {
    return lua::Userdata::check<WebSocket>(L, 1);
}

int NetLua::send(lua_State* L) {
    check(L).send(lua::Stack::read<std::string_view>(L, 2));
    return 0;
}

// Sends a Lua string as a binary message, byte for byte.
int NetLua::sendBinary(lua_State* L) {
    const std::string_view bytes = lua::Stack::read<std::string_view>(L, 2);
    check(L).sendBinary(std::span(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()));
    return 0;
}

int NetLua::ping(lua_State* L) {
    check(L).ping(lua_isnoneornil(L, 2) ? std::string_view{} : lua::Stack::read<std::string_view>(L, 2));
    return 0;
}

int NetLua::close(lua_State* L) {
    const auto code = static_cast<int>(luaL_optinteger(L, 2, 1000));
    check(L).close(code, lua_isnoneornil(L, 3) ? std::string_view{} : lua::Stack::read<std::string_view>(L, 3));
    return 0;
}

// Listens with on(event, function) to open, message (data, binary), pong (payload), disconnect (code, reason), reconnecting (attempt, delay), close (code, reason) or error (message), and returns a connection.
int NetLua::on(lua_State* L) {
    WebSocket& target = check(L);
    const std::string_view event = lua::Stack::read<std::string_view>(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    auto function = std::make_shared<lua::Reference>(L, 3);

    // clang-format off
    const auto call = [function](const auto& pushArguments) {
        lua_State* main = function->getState();
        lua::Runtime::runReporting(main, [&] {
            function->push(main);
            lua::Runtime::protectedCall(main, pushArguments(main), 0);
        });
    };
    // clang-format on

    core::Connection connection;
    if (event == "open") {
        connection = target.opened.connect([call] { call([](lua_State*) { return 0; }); });
    } else if (event == "message") {
        // clang-format off
        connection = target.received.connect([call](std::string_view data, bool binary) {
            call([&](lua_State* state) {
                lua_pushlstring(state, data.data(), data.size());
                lua_pushboolean(state, binary ? 1 : 0);
                return 2;
            });
        });
        // clang-format on
    } else if (event == "pong") {
        // clang-format off
        connection = target.ponged.connect([call](std::string_view payload) {
            call([&](lua_State* state) {
                lua_pushlstring(state, payload.data(), payload.size());
                return 1;
            });
        });
        // clang-format on
    } else if (event == "close") {
        // clang-format off
        connection = target.closed.connect([call](int code, std::string_view reason) {
            call([&](lua_State* state) {
                lua_pushinteger(state, code);
                lua_pushlstring(state, reason.data(), reason.size());
                return 2;
            });
        });
        // clang-format on
    } else if (event == "disconnect") {
        // clang-format off
        connection = target.disconnected.connect([call](int code, std::string_view reason) {
            call([&](lua_State* state) {
                lua_pushinteger(state, code);
                lua_pushlstring(state, reason.data(), reason.size());
                return 2;
            });
        });
        // clang-format on
    } else if (event == "reconnecting") {
        // clang-format off
        connection = target.reconnecting.connect([call](int attempt, float delay) {
            call([&](lua_State* state) {
                lua_pushinteger(state, attempt);
                lua_pushnumber(state, static_cast<lua_Number>(delay));
                return 2;
            });
        });
        // clang-format on
    } else if (event == "error") {
        // clang-format off
        connection = target.failed.connect([call](std::string_view message) {
            call([&](lua_State* state) {
                lua_pushlstring(state, message.data(), message.size());
                return 1;
            });
        });
        // clang-format on
    } else {
        return luaL_error(L, "Unknown WebSocket event '%s'. Sockets report open, message, pong, disconnect, reconnecting, close and error.", std::string(event).c_str());
    }
    lua::Userdata::emplace<core::Connection>(L, std::move(connection));
    return 1;
}

int NetLua::getState(lua_State* L) {
    lua::Stack::push(L, WebSocket::stateName(check(L).getState()));
    return 1;
}

int NetLua::getUrl(lua_State* L) {
    lua::Stack::push(L, check(L).getUrl());
    return 1;
}

int NetLua::getProtocol(lua_State* L) {
    lua::Stack::push(L, check(L).getProtocol());
    return 1;
}

int NetLua::getAttempt(lua_State* L) {
    lua::Stack::push(L, check(L).getAttempt());
    return 1;
}

int NetLua::openSockets(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getPlugin<plugins::NetPlugin>().getOpenSocketCount());
    return 1;
}

int NetLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"websocket", &lua::Binding::native<&websocket>},
        {"openSockets", &lua::Binding::native<&openSockets>},
        {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void NetLua::install(lua_State* L) {
    lua::ClassBuilder<WebSocket>(L).function("send", &lua::Binding::native<&send>).function("sendBinary", &lua::Binding::native<&sendBinary>).function("ping", &lua::Binding::native<&ping>).function("close", &lua::Binding::native<&close>).function("on", &lua::Binding::native<&on>).property("state", &getState).property("url", &getUrl).property("protocol", &getProtocol).property("attempt", &getAttempt).install();
    lua::Binding::preload(L, "haylen.net", &open);
}

} // namespace haylen::net
