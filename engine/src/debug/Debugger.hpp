#pragma once

#include <lua.hpp>

#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>

#include "debug/Tracer.hpp"
#include "haylen/core/Json.hpp"

namespace haylen::platform {
class Host;
}

namespace haylen::debug {

class HaltedStack;

// The Lua debugger of an app in development, which a client of the Debug Adapter Protocol, such as an editor, drives through the development server: breakpoints with conditions, counts of hits and log messages, pauses, steps, halts on errors that no protected call of the app catches, the stack with its scopes and values, and evaluations. It belongs to the development session, so its client, its breakpoints and the errors it watches outlive every app that restarts under it, and it follows the Lua state of the app that runs from `attach` to `detach`. A halt holds the frame thread inside the code of the app while the host keeps the platform responsive and the requests of the client still arrive, and nothing else of the app runs. A host whose frames cannot halt, such as the web, keeps the log points and refuses the breakpoints that halt.
class Debugger final {
  public:
    // Hands a message of the protocol to the client.
    using Send = std::function<void(const core::Json& message)>;

    // Delivers what the client sent since the last call, through `receive`, while the app halts.
    using Pump = std::function<void()>;

    // The time between two looks at the platform and the client while the app halts.
    static constexpr std::chrono::milliseconds kHaltPoll{10};

    // The one thread that the client sees, since the coroutines of Lua take turns on the frame thread.
    static constexpr int kThreadId = 1;

    Debugger(platform::Host& host, Send send, Pump pump);
    ~Debugger();

    Debugger(const Debugger&) = delete;
    Debugger& operator=(const Debugger&) = delete;

    // Queues a message of the client, which the next frame or the halt handles in order, on the frame thread.
    void receive(core::Json message);

    // Handles the requests that arrived while the app runs, at the start of a frame of the app whose Lua state is `L`.
    void update(lua_State* L);

    // Follows the Lua state of an app from before its code runs until it closes.
    void attach(lua_State* L);
    void detach() noexcept;

    // Forgets the client with its breakpoints, the errors it watches and its steps, and lets a halted app go on, such as when the client leaves or the connection to it ends.
    void reset();

    [[nodiscard]] bool isHalted() const noexcept {
        return stack != nullptr;
    }

    // Whether a client attached, which a halt needs.
    [[nodiscard]] bool isAttached() const noexcept {
        return attached;
    }

    // Halts the coroutine at the caller of the native function that runs, as `debug.breakpoint` asks, while a client is attached.
    void breakHere(lua_State* L);

    // Shows the text in the console of the client.
    void output(const std::string& text);

    // Holds the frame thread at the halt until the client lets the app go on, then sets the step it asked for. The tracer calls it.
    void halt(const Tracer::Halt& value);

    // The debugger that follows the Lua state, or null while none does.
    [[nodiscard]] static Debugger* find(lua_State* L);

  private:
    static constexpr const char* kRegistryKey = "haylen.debugger";

    // The exception filter of the protocol that halts on errors that no protected call of the app catches.
    static constexpr const char* kErrorFilter = "uncaught";

    // What the client asked the halted app to do next.
    enum class Resume : std::uint8_t {
        None,
        Continue,
        In,
        Over,
        Out,
    };

    void handle(const core::Json& request, lua_State* L);
    void dispatch(const core::Json& request, const core::Json& arguments, lua_State* L);
    void respond(const core::Json& request, core::Json body = core::Json::object());
    void fail(const core::Json& request, const std::string& message);
    void sendEvent(const std::string& event, core::Json body = core::Json::object());

    [[nodiscard]] core::Json getCapabilities() const;
    void setBreakpoints(const core::Json& request, const core::Json& arguments, lua_State* L);
    void setExceptionBreakpoints(const core::Json& request, const core::Json& arguments, lua_State* L);
    void evaluate(const core::Json& request, const core::Json& arguments, lua_State* L);
    void resume(const core::Json& request, Resume value);
    void pause(const core::Json& request, lua_State* L);

    // Reads a field of an object of a request, or an empty value or the fallback when it is missing. A field of another type than the protocol gives it throws `std::invalid_argument` with a message that names it, which fails the request.
    [[nodiscard]] static core::Json readObject(const core::Json& object, const char* key);
    [[nodiscard]] static core::Json readArray(const core::Json& object, const char* key);
    [[nodiscard]] static int readInt(const core::Json& object, const char* key, int fallback);
    [[nodiscard]] static std::string readText(const core::Json& object, const char* key);

    platform::Host& host;
    Send send;
    Pump pump;
    Tracer tracer{*this};
    lua_State* state = nullptr;
    std::deque<core::Json> inbox;
    std::int64_t sequence = 0;
    bool attached = false;

    // What exists only while the app halts: the halt, the view of its stack and what the client asked for next.
    Tracer::Halt halted;
    std::unique_ptr<HaltedStack> stack;
    Resume next = Resume::None;
};

} // namespace haylen::debug
