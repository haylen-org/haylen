#pragma once

#include <array>
#include <string_view>
#include <vector>

#include "haylen/core/Connection.hpp"
#include "haylen/core/EventBus.hpp"

struct lua_State;

namespace haylen::core {

// Installs haylen.signal, whose signals call Lua functions, and the Connection class that every listener registration returns.
class SignalLua final {
  public:
    static void install(lua_State* L);

    // Connects the function at index to the signal at index with the connect options at index, or 0 for none, and ties it to the owner at index, or 0 for none. Scenes listen through it.
    static Connection connect(lua_State* L, int signal, int function, int options, int owner);

    [[nodiscard]] static bool isSignal(lua_State* L, int index);

    // Lists every named signal that is still alive with its listener count, its emits and the listeners whose owner is gone.
    [[nodiscard]] static std::vector<EventBus::Topic> getNamedSignals(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 4> kConnectFields{"priority", "once", "deferred", "owner"};
    static constexpr const char* kNamedSignals = "haylen.namedSignals";

    static void pushArguments(lua_State* L, int first, int count);
    static int create(lua_State* L);
    static int connectFunction(lua_State* L);
    static int emit(lua_State* L);
    static int clear(lua_State* L);
    static int size(lua_State* L);
    static int emissionCount(lua_State* L);
    static int name(lua_State* L);
    static int blocked(lua_State* L);
    static int setBlocked(lua_State* L);
    static int list(lua_State* L);
    static int open(lua_State* L);

    static int connectionDisconnect(lua_State* L);
    static int connectionConnected(lua_State* L);
    static int connectionBlocked(lua_State* L);
    static int connectionSetBlocked(lua_State* L);
};

} // namespace haylen::core
