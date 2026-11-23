#pragma once

#include <array>
#include <memory>
#include <string_view>
#include <vector>

#include "core/ScriptedSignal.hpp"
#include "haylen/core/Connection.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/FrameQueue.hpp"
#include "haylen/core/Signal.hpp"
#include "haylen/lua/Reference.hpp"
#include "runtime/Owners.hpp"

namespace haylen::lua::core {

// Installs `haylen.signal`, whose signals call Lua functions, and the `Connection` class that every listener registration returns.
class SignalBinding final {
  public:
    static void install(lua_State* L);

    // Connects the function at index to the signal at index with the connect options at index, or 0 for none, and ties it to the owner at index, or 0 for none. Scenes listen through it.
    static haylen::core::Connection connect(lua_State* L, int signal, int function, int options, int owner);

    [[nodiscard]] static bool isSignal(lua_State* L, int index);

    // Fills `signals` with every named signal that is still alive with its listener count, its emits and the listeners whose owner is gone, in the storage it holds.
    static void readNamedSignals(lua_State* L, std::vector<haylen::core::EventBus::Topic>& signals);

  private:
    using Slot = haylen::core::Signal<const ScriptedSignal::Arguments&>::Slot;

    static constexpr std::array<std::string_view, 4> kConnectFields{"priority", "once", "deferred", "owner"};
    static constexpr const char* kNamedSignals = "haylen.namedSignals";

    static void pushArguments(lua_State* L, int first, int count);
    [[nodiscard]] static Slot makeSlot(std::shared_ptr<lua::Owners::Function> callback);
    [[nodiscard]] static Slot makeDeferredSlot(std::shared_ptr<lua::Owners::Function> callback, bool once, haylen::core::FrameQueue& queue);
    [[nodiscard]] static std::shared_ptr<lua::Reference> packArguments(const ScriptedSignal::Arguments& arguments);
    static void deliverPacked(const std::shared_ptr<lua::Owners::Function>& listener, const lua::Reference& packed, int count);

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

} // namespace haylen::lua::core
