#pragma once

#include <lua.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "haylen/lua/Type.hpp"
#include "haylen/platform/AppPlugin.hpp"
#include "haylen/platform/Bridge.hpp"

namespace varn::async {
class Promise;
}

namespace haylen::platform {

// Installs haylen.platform, which calls native methods, answers them from Lua, listens to native events and hands plugin modules the handles of their plugins, and the class of the calls that it and haylen.dialogs return.
class PlatformLua final {
  public:
    // A call as Lua holds it: the promise that its awaiters wait on, the error that it failed with and how the service that runs it gives it up, which returns whether it was still pending.
    struct Call {
        std::shared_ptr<varn::async::Promise> promise;
        std::uint64_t id = 0;
        std::optional<Bridge::Error> error;
        bool (*cancel)(lua_State* L, std::uint64_t id) = nullptr;
    };

    static void install(lua_State* L);

  private:
    static constexpr const char* kErrorType = "haylen.PlatformError";
    static constexpr std::array<std::string_view, 1> kCallOptions{"timeout"};
    static constexpr std::array<std::string_view, 1> kEmitOptions{"retain"};

    // Starts a call of the method with the parameters and options at the given stack indexes, and pushes the call.
    static void pushCall(lua_State* L, const std::string& method, int paramsIndex, int optionsIndex);

    // Connects the listener at the given stack index to the native event, and pushes the connection.
    static void pushConnection(lua_State* L, const std::string& event, int listenerIndex);

    [[nodiscard]] static bool cancelBridgeCall(lua_State* L, std::uint64_t id);

    static int call(lua_State* L);
    static int await(lua_State* L);
    static int finishAwait(lua_State* L, int status, lua_KContext context);
    static int cancel(lua_State* L);
    static int getId(lua_State* L);
    static int isDone(lua_State* L);
    static int getPromise(lua_State* L);
    static int resolve(lua_State* L);
    static int emit(lua_State* L);
    static int pendingCallCount(lua_State* L);
    static int on(lua_State* L);
    static int send(lua_State* L);
    static int registerHandler(lua_State* L);
    static int hasHandler(lua_State* L);
    static int plugins(lua_State* L);
    static int plugin(lua_State* L);
    static int getPluginId(lua_State* L);
    static int getPluginVersion(lua_State* L);
    static int getPluginConfig(lua_State* L);
    static int isPluginNative(lua_State* L);
    [[nodiscard]] static std::string getPluginName(lua_State* L);
    static int callPlugin(lua_State* L);
    static int sendToPlugin(lua_State* L);
    static int onPlugin(lua_State* L);
    static int open(lua_State* L);

    // Pushes a table with message, code and data that reads as its message in tostring and concatenation.
    static void pushError(lua_State* L, const Bridge::Error& error);
    static int errorToString(lua_State* L);
    static int concatError(lua_State* L);
};

} // namespace haylen::platform

namespace haylen::lua {

template <> struct Type<platform::PlatformLua::Call> {
    static constexpr const char* name = "haylen.PlatformCall";
    using Storage = std::shared_ptr<platform::PlatformLua::Call>;
};

template <> struct Type<platform::AppPlugin> {
    static constexpr const char* name = "haylen.AppPlugin";
    using Storage = platform::AppPlugin;
};

} // namespace haylen::lua
