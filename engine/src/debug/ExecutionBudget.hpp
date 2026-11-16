#pragma once

#include <lua.hpp>

#include <chrono>
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

#include "haylen/lua/Error.hpp"

namespace haylen::debug {

// What the Lua code of an app in development may use: the time it may run each time the host enters the engine, such as for a frame, the start of the app or an event, so code that never ends stops the app instead of freezing the platform, and the memory of its Lua state. A count hook on every coroutine reads the clock every few hundred instructions, and the allocator of the state refuses memory past the budget, which runs the budget out unless an emergency collection of Lua makes room for the growth. Once the time or the memory ran out, the budget cancels the app through the engine with an error that names the code that ran out of it, and from then on every instruction of the app raises that error again. So no `pcall`, `xpcall` or coroutine of the app keeps it, and only a restart runs the app again. Only the host sets the time, and an app in development has no way to reach it. Only an app in development has a budget, so an app that ships never hooks its Lua for it.
class ExecutionBudget final {
  public:
    // Cancels the app with the error of the budget that ran out.
    using Cancel = std::function<void(const lua::Error& error)>;

    // Names the step of the engine that runs now, such as `update` or `reload`, or answers an empty text where the step of the host names it.
    using StageReader = std::function<std::string_view()>;

    // A call of the host into the engine, which the budget times from its start to its end under the name of its step. An entry inside another one keeps the time of the outer one.
    class Entry final {
      public:
        Entry(ExecutionBudget* owner, std::string_view stage) noexcept;
        ~Entry();

        Entry(const Entry&) = delete;
        Entry& operator=(const Entry&) = delete;

      private:
        ExecutionBudget* budget;
    };

    // The instructions between two looks at the clock, few enough that a look comes well within a millisecond and many enough that the clock costs nothing that shows.
    static constexpr int kCheckInstructions = 400;

    ExecutionBudget() = default;
    ~ExecutionBudget();

    ExecutionBudget(const ExecutionBudget&) = delete;
    ExecutionBudget& operator=(const ExecutionBudget&) = delete;

    // What the budget allows: the time of each entry and the bytes of the Lua state.
    struct Limits {
        std::chrono::milliseconds time{};
        std::size_t memory = 0;
    };

    // Starts timing the Lua state of an app with the limits, hooks every coroutine it holds, while new coroutines take the hook of the one that creates them, and counts its memory from what it holds already. A new app starts with a whole budget.
    void attach(lua_State* L, Limits value, Cancel cancel, StageReader stage);
    void detach() noexcept;

    // Keeps timing the Lua state while the app stops and its state closes, but cancels no app any more.
    void finishApp() noexcept;

    [[nodiscard]] bool isExhausted() const noexcept {
        return exhausted;
    }

    // The hook mask and count that every coroutine of the attached state needs, which the tracer of the debugger adds to its own hooks, or none while no state is attached.
    [[nodiscard]] static int getMask() noexcept;
    [[nodiscard]] static int getCount() noexcept;

    // Gives the coroutine the hook of the budget alone, or no hook while no state is attached, such as once the tracer no longer watches it.
    static void hook(lua_State* L);

    // Looks at the clock on a count event of any hook of the coroutine, and raises the error of the budget once it ran out.
    static void check(lua_State* L);

    // Gives the entry that runs its whole time again, such as once a halt of the debugger ended, since a halt waits for the person.
    static void renew() noexcept;

    // Replaces `xpcall` with one whose message handler of the app no longer runs once the budget ran out. Lua calls a message handler with the hooks off when the error rises inside a hook, so a handler that never ends would otherwise escape the budget.
    static void guardMessageHandlers(lua_State* L);

  private:
    using Clock = std::chrono::steady_clock;

    // The budget that the hooks of the attached state reach. One engine runs at a time in a process.
    static inline ExecutionBudget* current = nullptr;

    static void onHook(lua_State* L, lua_Debug* ar);
    static int callGuarded(lua_State* L);
    static int finishGuarded(lua_State* L, int status, lua_KContext context);
    static int handleGuarded(lua_State* L);
    static void* allocate(void* data, void* block, std::size_t oldSize, std::size_t newSize);
    void exhaust(lua_State* L, const std::string& reason, lua::Error::Code code);
    [[nodiscard]] std::string readCurrentStage() const;
    [[nodiscard]] std::string describeMemory() const;
    [[nodiscard]] static std::string describe(const std::string& reason, const std::string& stage);

    // Cancels an app whose memory ran out where no code of the app ran again to raise it, such as when the failed allocation ended the call of the host.
    void cancelForMemory() noexcept;

    lua_State* state = nullptr;
    Limits limits;
    lua_Alloc baseAllocator = nullptr;
    void* baseData = nullptr;
    std::size_t used = 0;

    // Whether the allocator refused memory and no growth succeeded since, as one does when Lua asks again after an emergency collection that made room.
    bool memoryExhausted = false;
    std::string memoryStage;
    Cancel cancelApp;
    StageReader readStage;
    Clock::time_point deadline = Clock::time_point::max();
    std::string_view entryStage;
    int entries = 0;
    bool exhausted = false;
    std::string message;
};

} // namespace haylen::debug
