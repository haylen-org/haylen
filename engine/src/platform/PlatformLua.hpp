#pragma once

#include <lua.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

#include "haylen/lua/Type.hpp"
#include "haylen/platform/Bridge.hpp"

namespace varn::async {
class Promise;
}

namespace haylen::platform {

// Installs haylen.platform, which calls native methods, answers them from Lua and listens to native events.
class PlatformLua final {
  public:
    // A platform call as Lua holds it: the promise that its awaiters wait on, and the error that it failed with.
    struct Call {
        std::shared_ptr<varn::async::Promise> promise;
        std::uint64_t id = 0;
        std::optional<Bridge::Error> error;
    };

    static void install(lua_State* L);

  private:
    static constexpr const char* kErrorType = "haylen.PlatformError";
    static constexpr std::array<std::string_view, 1> kCallOptions{"timeout"};

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
    static int registerHandler(lua_State* L);
    static int hasHandler(lua_State* L);
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

} // namespace haylen::lua
