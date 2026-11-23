#pragma once

#include <lua.hpp>

#include <exception>
#include <string>
#include <string_view>
#include <type_traits>

#include "haylen/core/AppError.hpp"

namespace haylen::core {
class Engine;
}

namespace haylen::debug {
class Tracer;
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

    // Loads text-only Lua source and runs it. Throws `core::AppError` with the message and the stack when loading or running fails.
    static void runChunk(lua_State* L, std::string_view source, const std::string& chunkName);

    // Calls the function below the arguments on the stack with the engine's message handler. Throws `core::AppError` with the stack of the failing call when the call raises an error.
    static void protectedCall(lua_State* L, int arguments, int results);

    // Runs `body` inside a protected Lua call, so errors that C API calls raise, such as a failing setter behind `lua_setfield`, become an `core::AppError` with its stack. Native code outside any Lua call uses it before touching Lua values.
    template <typename Body> static void protectedRun(lua_State* L, Body&& body) {
        Call call{.body = &body, .invoke = &invoke<std::remove_reference_t<Body>>};
        runProtected(L, call);
    }

    // Describes the stack of `L` from `level` down as the frames of an error with the given text, leaving out the engine's own protected-call and task levels. Level 0 is the running function.
    [[nodiscard]] static core::AppError captureError(lua_State* L, const std::string& text, int level);

    // Reports a script failure to the engine that owns the state, which shows it on the error screen. An `core::AppError` keeps its stack.
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
    friend class ModuleGraph;
    friend class Task;
    friend class debug::Tracer;

    static constexpr const char* kEngineKey = "haylen.engine";
    static constexpr const char* kErrorType = "haylen.Error";
    static constexpr const char* kKeptFailureKey = "haylen.keptFailure";
    static constexpr const char* kSyntaxErrorsKey = "haylen.syntaxErrors";
    static constexpr int kMaxSyntaxErrors = 64;

    // A deeper stack keeps only its innermost and outermost levels, like a Lua traceback, so a runaway recursion reports quickly.
    static constexpr int kInnerLevels = 10;
    static constexpr int kOuterLevels = 11;

    // A level of a stack with the fields that `lua_getinfo` fills for the options "Sln", which the protected calls read from the stack and Varn lists in the frames of a failure.
    struct Level {
        std::string_view source;
        int line = 0;
        std::string_view what;
        std::string_view nameWhat;
        std::string_view name;
        int lineDefined = 0;
    };

    // The message handler of every protected call: it replaces the error value with an `core::AppError` userdata that holds the stack.
    static int handleMessage(lua_State* L);

    // Calls the function below the arguments under a protected call that keeps the failure where it rises, before its stack unwinds: the error value, the `core::AppError` with that stack and the failure that the tasks of Varn read. It returns the status of the call and leaves the kept failure, or the error value of a memory error, on top, so native code undoes what it started before `raiseKept` raises the same error value again.
    [[nodiscard]] static int keptCall(lua_State* L, int arguments, int results);
    static int raiseKept(lua_State* L, int status);
    static int keepFailure(lua_State* L);

    // Pushes the `core::AppError` of the kept failure whose error value is the value at index, and answers whether it did. The next message handler that an error reaches takes the kept failure, so a call forgets it whatever it answers.
    [[nodiscard]] static bool takeKept(lua_State* L, int index);

    // Pushes the `core::AppError` of the error value at index 1 of a message handler, with the stack of the code where it rose.
    static void pushCaptured(lua_State* L);

    // Records the message of a module of the package that did not compile, which `require` raises as it is, so the error keeps the code `syntax` once it reaches a protected call of the engine. The record keeps the latest messages only.
    static void recordSyntaxError(lua_State* L, int index);
    [[nodiscard]] static bool isSyntaxError(lua_State* L, int index);

    // Returns the `core::AppError` that `handleMessage` left at index, or an `core::AppError` without frames for any other error value.
    [[nodiscard]] static core::AppError readError(lua_State* L, int index);
    [[nodiscard]] static std::string describeValue(lua_State* L, int index);
    [[nodiscard]] static core::AppError::Frame makeFrame(const Level& level);
    [[nodiscard]] static core::AppError::Frame makeSkippedFrame(int count);
    [[nodiscard]] static std::string describeFunction(const Level& level);
    [[nodiscard]] static core::AppError::Frame::Kind getFrameKind(std::string_view what) noexcept;
    [[nodiscard]] static int findLastLevel(lua_State* L);
    static void pushError(lua_State* L, core::AppError error);
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
