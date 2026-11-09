#pragma once

#include <lua.hpp>

#include <chrono>
#include <string>
#include <string_view>

namespace haylen::debug {

// Runs an expression or a statement that the person typed in the debugger, in a given environment, on a coroutine of its own with a time bound, so code that never ends fails instead of holding the debugger. It is the one way the debugger runs code of the app, and only when the person asks for it, such as an evaluation, the condition of a breakpoint or the message of a log point.
class Evaluation final {
  public:
    // How long an evaluation may run, which code that loops or waits on something that never comes exceeds.
    static constexpr std::chrono::milliseconds kTimeLimit{1000};

    // What an evaluation did: whether it ran, the error that stopped it, and how many results it left on the stack.
    struct Result {
        bool ok = false;
        std::string error;
        int count = 0;
    };

    // Compiles the text as an expression whose values return, or else as statements, gives it the table at `environment` as its `_ENV` and runs it. On success it pushes its results onto the stack of `L`.
    [[nodiscard]] static Result run(lua_State* L, int environment, std::string_view source, std::chrono::milliseconds limit = kTimeLimit);

    // Describes an error value the way the stand-alone interpreter does without running a metamethod: strings and numbers are the message, and other values name their type.
    [[nodiscard]] static std::string describeError(lua_State* L, int index);

  private:
    // The instructions between two looks at the clock.
    static constexpr int kCheckInstructions = 1000;

    static void onCount(lua_State* L, lua_Debug* ar);

    // The time the evaluation that runs must end by. Evaluations run on the frame thread one at a time.
    static inline std::chrono::steady_clock::time_point deadline;
    static inline std::chrono::milliseconds allowed{0};
    static inline bool running = false;

    // The hook of the code that started the evaluation, which the coroutines that the evaluation created take once it ended.
    static inline lua_Hook appHook = nullptr;
    static inline int appMask = 0;
    static inline int appCount = 0;
};

} // namespace haylen::debug
