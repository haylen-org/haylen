#pragma once

#include <lua.hpp>

#include <functional>
#include <memory>
#include <optional>

#include "haylen/lua/Error.hpp"

namespace haylen::lua {

// A Lua function that runs in a coroutine of its own, waits on promises with `:await()` like a task of `async.spawn` and can be cancelled like one. A function that returns a promise finishes once the promise settles.
class Task final : public std::enable_shared_from_this<Task> {
  public:
    // Receives the error of a task that failed, or nothing once it finished.
    using Completion = std::function<void(const std::optional<Error>& error)>;

    Task(lua_State* L, Completion onFinish);
    ~Task();

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    // Runs the function at index with the given number of arguments above it until it first waits. An owner index other than 0 gives the task to that owner before it runs, so a task that releases its own owner stops too.
    static std::shared_ptr<Task> start(lua_State* L, int function, int arguments, int owner, Completion onFinish);

    // Stops the task for good the way `task.cancel()` of Varn stops a task: its code never runs again, even when a promise it waits for settles later, and its coroutine closes, which runs its pending to-be-closed variables. A task cancelled while its own code runs stops at its next wait.
    void cancel();
    [[nodiscard]] bool isRunning() const noexcept {
        return !finished && !cancelled;
    }

  private:
    friend class Runtime;

    static constexpr const char* kHolderType = "haylen.TaskHolder";
    static constexpr lua_KContext kCalled = 0;
    static constexpr lua_KContext kAwaited = 1;

    static int body(lua_State* L);
    static int continueBody(lua_State* L, int status, lua_KContext context);
    static int collectHolder(lua_State* L);

    void run(lua_State* L, int function, int arguments);
    void finish(const std::optional<Error>& error);
    void release() noexcept;

    lua_State* main;
    lua_State* thread = nullptr;
    int threadReference = LUA_NOREF;
    Completion completion;
    bool cancelled = false;
    bool finished = false;
};

} // namespace haylen::lua
