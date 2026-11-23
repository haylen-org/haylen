#pragma once

#include <lua.hpp>

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "haylen/core/JobSystem.hpp"
#include "haylen/core/Json.hpp"

namespace varn::async {
class Promise;
}

namespace haylen::core {
class Engine;
}

namespace haylen::lua {

// A result that Lua waits for with `promise:await()` inside a coroutine. Native code creates it, returns it to Lua with `push` and settles it once. It may be settled from any thread while the engine runs, and the waiting coroutine resumes on the frame thread.
class Promise final {
  public:
    explicit Promise(haylen::core::Engine& engine);

    // Returns whether the value at index is a promise of Varn, whoever created it.
    [[nodiscard]] static bool isPromise(lua_State* L, int index);

    void push(lua_State* L) const;

    // Rejects the promise instead when Lua cannot hold the value, which JSON nested more than 128 levels deep cannot.
    void resolve(haylen::core::Json value) const;

    // The function runs on the frame thread when the coroutine resumes and pushes exactly one value.
    void resolveWith(std::function<void(lua_State* L)> pushValue) const;

    // Settles the promise with the result of a job on the frame thread: it rejects with the error of a job that failed, or resolves with the value the job made, which `push(L, value)` pushes for every coroutine that awaits it.
    template <typename T, typename Push> void settle(haylen::core::JobSystem::Result<T> result, Push push) const {
        if (!result.isOk()) {
            reject(std::move(result.error));
            return;
        }

        const auto value = std::make_shared<const T>(std::move(*result.value));
        resolveWith([push = std::move(push), value](lua_State* L) { push(L, *value); });
    }

    // A rejection that nothing awaits or catches stops the app like a failed task, as `async.onFailure` receives it.
    void reject(std::string message) const;

    // Rejects with a failure that reaches the app another way, such as through an error handler or the log, or that the app asked for, such as a cancellation, so the rejection never stops the app when nothing awaits it.
    void rejectHandled(std::string message) const;

    // Rejects with the table at the top of the stack, which describes the failure with its message in the field `error` and anything else the app may act on, and pop it. Awaiters receive `nil`, the message and the table. They run on the frame thread, and the handled one never stops the app when nothing awaits it, like `rejectHandled`.
    void rejectFailure(lua_State* L) const;
    void rejectHandledFailure(lua_State* L) const;
    [[nodiscard]] bool isSettled() const;

  private:
    // The metatable name of the promises of Varn.
    static constexpr const char* kVarnType = "varn.Promise";

    std::shared_ptr<varn::async::Promise> promise;
};

} // namespace haylen::lua
