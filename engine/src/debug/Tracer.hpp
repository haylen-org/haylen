#pragma once

#include <lua.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "debug/Breakpoints.hpp"
#include "haylen/lua/Reference.hpp"

namespace haylen::debug {

class Debugger;

// Follows the Lua code of the app for the debugger through the hooks of Lua, and halts it where a breakpoint, a step, a pause or an error that no protected call of the app catches asks for it. Lua sets hooks per coroutine, so the tracer hooks every coroutine of the heap when it starts to watch, and new coroutines take the hook of the one that creates them. Calls and returns hook a coroutine only while something is watched, and lines only in the functions whose lines hold a breakpoint or while a step needs them, so code without breakpoints runs at full speed. A coroutine whose hook is no longer needed drops it at its next event.
class Tracer final {
  public:
    // What the app does until it halts again: it runs, it halts at the next line that runs anywhere, or it halts at the next line of the coroutine that halted, also inside a function it calls or a coroutine it resumes, at the same depth or above, or above it.
    enum class Step : std::uint8_t {
        None,
        Pause,
        In,
        Over,
        Out,
    };

    // Why the app halts, which the client shows by the names of the protocol.
    enum class Reason : std::uint8_t {
        Breakpoint,
        Step,
        Pause,
        Error,
    };
    static constexpr std::array<std::string_view, 4> kReasonNames = {"breakpoint", "step", "pause", "exception"};

    // A halt: the coroutine, the level of its stack where the code of the app starts, why it halts, a text that explains it and the ids of the breakpoints that halted it.
    struct Halt {
        lua_State* thread = nullptr;
        int level = 0;
        Reason reason = Reason::Pause;
        std::string text;
        std::vector<int> breakpoints;
    };

    explicit Tracer(Debugger& owner) noexcept : debugger(owner) {}
    ~Tracer();

    Tracer(const Tracer&) = delete;
    Tracer& operator=(const Tracer&) = delete;

    // Starts and ends following the Lua state of an app, which the hooks of every coroutine reach until it closes.
    void attach(lua_State* L);
    void detach() noexcept;

    [[nodiscard]] Breakpoints& getBreakpoints() noexcept {
        return breakpoints;
    }

    // Halts where an error rises that no protected call of the app catches, before its stack unwinds.
    void setErrorsWatched(bool value);

    // Sets what the app does after a halt in the coroutine, whose stack the code of the app starts at the level, or from the next frame on with no coroutine.
    void setStep(Step value, lua_State* thread = nullptr, int level = 0);

    // Hooks every coroutine once something is watched, and takes the hooks of the coroutine at its current function as they are now. Changes of the breakpoints call it.
    void refresh(lua_State* L);

    // Halts nothing until the next frame, such as after the platform asked the frame to finish during a halt.
    void suspendUntilFrame() noexcept {
        suspended = true;
    }
    void beginFrame() noexcept {
        suspended = false;
    }

    // Halts the coroutine at the level of the caller of a native function, as `debug.breakpoint` asks.
    void haltHere(lua_State* L, int level, std::string text);

  private:
    // The tracer that the hooks of the attached state reach. One engine runs at a time in a process.
    static inline Tracer* current = nullptr;

    // Keeps the hooks from halting while the tracer runs code itself or the app halts, also when an error leaves early.
    class Busy final {
      public:
        explicit Busy(Tracer& owner) noexcept : tracer(owner) {
            tracer.busy = true;
        }
        ~Busy() {
            tracer.busy = false;
        }

        Busy(const Busy&) = delete;
        Busy& operator=(const Busy&) = delete;

      private:
        Tracer& tracer;
    };

    static void onHook(lua_State* L, lua_Debug* ar);

    [[nodiscard]] bool isWatching() const noexcept {
        return !breakpoints.empty() || errorsWatched || step != Step::None;
    }
    [[nodiscard]] bool needsReturns() const noexcept {
        return !breakpoints.empty() || step != Step::None;
    }

    // Whether every line of the coroutine counts, whatever function it runs, because a step or a pause needs them.
    [[nodiscard]] bool needsAllLines(lua_State* L) const noexcept;

    // The hooks that the coroutine needs while it runs the function of the level that `info` describes, which `lua_getinfo` filled with "S".
    [[nodiscard]] int getMask(lua_State* L, const lua_Debug& info);
    void applyMask(lua_State* L, int mask);
    void hookAll(lua_State* L);

    void enter(lua_State* L, lua_Debug* ar);
    void leave(lua_State* L, lua_Debug* ar);
    void line(lua_State* L, lua_Debug* ar);

    [[nodiscard]] bool isStepDone(lua_State* L);
    [[nodiscard]] static int getDepth(lua_State* L);

    // Whether a breakpoint whose line runs halts, after its condition and its count of hits, which writes the message of a log point instead.
    [[nodiscard]] bool takeBreakpoint(lua_State* L, Breakpoints::Breakpoint& breakpoint);
    [[nodiscard]] std::string interpolate(lua_State* L, const std::string& message);

    void halt(Halt value);

    Debugger& debugger;
    Breakpoints breakpoints;
    lua_State* state = nullptr;
    lua_State* main = nullptr;
    bool errorsWatched = false;
    bool hooked = false;
    bool suspended = false;

    // Whether the tracer runs code itself, such as the condition of a breakpoint, or the app halts, while no hook may halt again.
    bool busy = false;

    Step step = Step::None;
    lua_State* stepThread = nullptr;
    int stepDepth = 0;

    // Holds the coroutine of the step, so the collector never frees a coroutine that a step waits for, such as one that yields and is dropped.
    lua::Reference stepAnchor;
};

} // namespace haylen::debug
