#pragma once

#include <lua.hpp>

#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "haylen/lua/Error.hpp"

namespace haylen::core {
class Engine;
}

namespace haylen::lua {

class Environment;
class Task;

// Runs Lua code for the engine and reaches the engine that owns a Lua state.
class Runtime final {
  public:
    // Returns the engine that owns the Lua state. Every engine module is installed into a state that has one.
    [[nodiscard]] static core::Engine& getEngine(lua_State* L);

    // Returns the main thread of the state. Callbacks that outlive the call that registered them run on it, because the coroutine that registered them may be gone by then.
    [[nodiscard]] static lua_State* getMainThread(lua_State* L) noexcept;

    // Loads text-only Lua source and runs it. Throws `Error` with the message and the stack when loading or running fails.
    static void runChunk(lua_State* L, std::string_view source, const std::string& chunkName);

    // Calls the function below the arguments on the stack with the engine's message handler. Throws `Error` with the stack of the failing call when the call raises an error.
    static void protectedCall(lua_State* L, int arguments, int results);

    // Runs `body` inside a protected Lua call, so errors that C API calls raise, such as a failing setter behind `lua_setfield`, become an `Error` with its stack. Native code outside any Lua call uses it before touching Lua values.
    template <typename Body> static void protectedRun(lua_State* L, Body&& body) {
        Call call{.body = &body, .invoke = &invoke<std::remove_reference_t<Body>>};
        runProtected(L, call);
    }

    // Describes the stack of `L` from `level` down as the frames of an error with the given text, leaving out the engine's own protected-call and task levels. Level 0 is the running function.
    [[nodiscard]] static Error captureError(lua_State* L, const std::string& text, int level);

    // Reports a script failure to the engine that owns the state, which shows it on the error screen. An `Error` keeps its stack.
    static void reportError(lua_State* L, const std::exception& exception);

    // Runs `body` and reports a failure to the engine instead of letting it escape into the native code that triggered the callback.
    template <typename Body> static void runReporting(lua_State* L, Body&& body) {
        try {
            body();
        } catch (const std::exception& exception) {
            reportError(L, exception);
        }
    }

  private:
    friend class Environment;
    friend class Task;

    static constexpr const char* kEngineKey = "haylen.engine";
    static constexpr const char* kErrorType = "haylen.Error";

    // A deeper stack keeps only its innermost and outermost levels, like a Lua traceback, so a runaway recursion reports quickly.
    static constexpr int kInnerLevels = 10;
    static constexpr int kOuterLevels = 11;

    // The message handler of every protected call: it replaces the error value with an `Error` userdata that holds the stack.
    static int handleMessage(lua_State* L);

    // Returns the `Error` that `handleMessage` left at index, or an `Error` without frames for any other error value.
    [[nodiscard]] static Error readError(lua_State* L, int index);
    [[nodiscard]] static std::string describeValue(lua_State* L, int index);
    [[nodiscard]] static std::string describeFunction(const lua_Debug& info);
    [[nodiscard]] static Error::Frame::Kind getFrameKind(const lua_Debug& info) noexcept;
    [[nodiscard]] static int findLastLevel(lua_State* L);

    // Reads the frames of a traceback that `luaL_traceback` wrote, innermost first, for failures whose stack is gone by the time the engine hears of them.
    [[nodiscard]] static std::vector<Error::Frame> readTraceback(std::string_view traceback);
    [[nodiscard]] static std::optional<Error::Frame> readTracebackLine(std::string_view line);
    static void pushError(lua_State* L, Error error);
    static int collectError(lua_State* L);
    static int errorToString(lua_State* L);
    static int runBody(lua_State* L);

    // A body for `protectedRun`, reduced to its address and the function that calls it, so running it allocates nothing.
    struct Call {
        const void* body = nullptr;
        void (*invoke)(const void* body, lua_State* L) = nullptr;
    };

    template <typename Body> static void invoke(const void* body, lua_State* L) {
        (*static_cast<const Body*>(body))(L);
    }
    static void runProtected(lua_State* L, Call& call);
};

} // namespace haylen::lua
