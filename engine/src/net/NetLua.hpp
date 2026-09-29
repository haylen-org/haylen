#pragma once

#include <lua.hpp>

#include <array>
#include <memory>
#include <string_view>

#include "haylen/lua/Type.hpp"
#include "haylen/net/WebSocket.hpp"

namespace haylen::lua {

template <> struct Type<net::WebSocket> {
    static constexpr const char* name = "haylen.WebSocket";
    using Storage = std::shared_ptr<net::WebSocket>;
};

} // namespace haylen::lua

namespace haylen::net {

// Installs haylen.net, which opens WebSockets, and the WebSocket class.
class NetLua final {
  public:
    static void install(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 3> kSocketOptions{"protocols", "maxMessageSize", "reconnect"};
    static constexpr std::array<std::string_view, 5> kReconnectOptions{"initialDelay", "maxDelay", "multiplier", "jitter", "maxAttempts"};
    static constexpr std::array<std::string_view, 1> kListenerOptions{"owner"};

    [[nodiscard]] static WebSocket& check(lua_State* L);
    [[nodiscard]] static WebSocket::Reconnect readReconnect(lua_State* L, int index);

    static int connectWebSocket(lua_State* L);
    static int openSocketCount(lua_State* L);
    static int send(lua_State* L);
    static int sendBinary(lua_State* L);
    static int ping(lua_State* L);
    static int close(lua_State* L);
    static int on(lua_State* L);
    static int getState(lua_State* L);
    static int getUrl(lua_State* L);
    static int getProtocol(lua_State* L);
    static int getAttempt(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::net
