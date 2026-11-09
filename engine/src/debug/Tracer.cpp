#include "debug/Tracer.hpp"

#include <utility>

#include "debug/Debugger.hpp"
#include "debug/Evaluation.hpp"
#include "debug/HaltedStack.hpp"
#include "debug/ThreadWalk.hpp"
#include "haylen/lua/Runtime.hpp"
#include "varn/async/AsyncModule.h"

namespace haylen::debug {

Tracer::~Tracer() {
    detach();
}

void Tracer::attach(lua_State* L) {
    state = L;
    main = lua::Runtime::getMainThread(L);
    current = this;
    hooked = false;
    refresh(L);
}

void Tracer::detach() noexcept {
    if (current == this) {
        current = nullptr;
    }
    state = nullptr;
    main = nullptr;
    hooked = false;
    step = Step::None;
    stepThread = nullptr;
    stepAnchor.reset();
}

void Tracer::setErrorsWatched(bool value) {
    errorsWatched = value;
}

void Tracer::setStep(Step value, lua_State* thread, int level) {
    step = value;
    stepThread = thread;
    stepDepth = thread != nullptr ? getDepth(thread) - level : 0;
    stepAnchor.reset();
    if (thread != nullptr) {
        lua_pushthread(thread);
        stepAnchor = lua::Reference(thread, -1);
        lua_pop(thread, 1);
    }
}

// A coroutine that stops being watched drops its hook at its next event, so ending a watch costs no walk of the heap, while starting one does.
void Tracer::refresh(lua_State* L) {
    if (state == nullptr) {
        return;
    }
    if (!isWatching()) {
        hooked = false;
        return;
    }
    if (!hooked) {
        hookAll(L);
        hooked = true;
        return;
    }
    lua_Debug info{};
    if (lua_getstack(L, 0, &info) != 0 && lua_getinfo(L, "S", &info) != 0) {
        applyMask(L, getMask(L, info));
    }
}

void Tracer::hookAll(lua_State* L) {
    const int base = LUA_MASKCALL | (needsReturns() ? LUA_MASKRET : 0);
    // clang-format off
    const auto hook = [this, base](lua_State* thread) {
        applyMask(thread, base | (needsAllLines(thread) ? LUA_MASKLINE : 0));
    };
    // clang-format on
    hook(main);
    if (L != main) {
        hook(L);
    }
    ThreadWalk::run(L, hook);

    // The coroutine that runs now goes on in its current function, which no call announces.
    lua_Debug info{};
    if (lua_getstack(L, 0, &info) != 0 && lua_getinfo(L, "S", &info) != 0) {
        applyMask(L, getMask(L, info));
    }
}

bool Tracer::needsAllLines(lua_State* L) const noexcept {
    switch (step) {
    case Step::Pause:
    case Step::In:
        return true;
    case Step::Over:
    case Step::Out:
        return L == stepThread;
    case Step::None:
        return false;
    }
    return false;
}

// The main chunk of a file spans the whole file, and the lines of a function span its definition.
int Tracer::getMask(lua_State* L, const lua_Debug& info) {
    const int mask = LUA_MASKCALL | (needsReturns() ? LUA_MASKRET : 0);
    if (needsAllLines(L)) {
        return mask | LUA_MASKLINE;
    }
    if (breakpoints.empty() || info.what[0] == 'C') {
        return mask;
    }
    const Breakpoints::File* file = breakpoints.find(info.source);
    if (file == nullptr) {
        return mask;
    }
    const bool covered = info.what[0] == 'm' || file->covers(info.linedefined, info.lastlinedefined);
    return covered ? mask | LUA_MASKLINE : mask;
}

void Tracer::applyMask(lua_State* L, int mask) {
    if (lua_gethook(L) != &onHook || lua_gethookmask(L) != mask) {
        lua_sethook(L, &onHook, mask, 0);
    }
}

void Tracer::onHook(lua_State* L, lua_Debug* ar) {
    Tracer* tracer = current;
    if (tracer == nullptr || !tracer->isWatching()) {
        lua_sethook(L, nullptr, 0, 0);
        return;
    }
    if (tracer->busy) {
        return;
    }
    switch (ar->event) {
    case LUA_HOOKCALL:
    case LUA_HOOKTAILCALL:
        tracer->enter(L, ar);
        break;
    case LUA_HOOKRET:
        tracer->leave(L, ar);
        break;
    case LUA_HOOKLINE:
        tracer->line(L, ar);
        break;
    default:
        break;
    }
}

// Lua calls the message handler of a protected call where an error rises, before the stack unwinds, so a call of the handler of the engine or of the tasks of Varn is an error that no protected call of the app caught. The error value is the argument of the handler.
void Tracer::enter(lua_State* L, lua_Debug* ar) {
    if (!errorsWatched || suspended) {
        lua_getinfo(L, "S", ar);
        applyMask(L, getMask(L, *ar));
        return;
    }

    lua_getinfo(L, "Sf", ar);
    const lua_CFunction function = lua_tocfunction(L, -1);
    lua_pop(L, 1);
    applyMask(L, getMask(L, *ar));
    if (function != &lua::Runtime::handleMessage && function != &varn::async::AsyncModule::capture) {
        return;
    }
    std::string text = "An error rose that no protected call of the app catches.";
    if (lua_getlocal(L, ar, 1) != nullptr) {
        text = Evaluation::describeError(L, -1);
        lua_pop(L, 1);
    }
    halt({.thread = L, .level = 1, .reason = Reason::Error, .text = std::move(text)});
}

// A coroutine whose last level returns ends, so the step of a coroutine that ended halts at the next line anywhere, while the main thread goes back to the engine and halts at its next line of the app.
void Tracer::leave(lua_State* L, lua_Debug*) {
    lua_Debug caller{};
    if (lua_getstack(L, 1, &caller) == 0) {
        if (step != Step::None && L == stepThread && L != main) {
            step = Step::Pause;
            stepThread = nullptr;
            stepAnchor.reset();
        }
        return;
    }
    lua_getinfo(L, "S", &caller);
    applyMask(L, getMask(L, caller));
}

void Tracer::line(lua_State* L, lua_Debug* ar) {
    if (suspended) {
        return;
    }
    if (breakpoints.hasLine(ar->currentline)) {
        lua_getinfo(L, "S", ar);
        if (Breakpoints::File* file = breakpoints.find(ar->source)) {
            Breakpoints::Breakpoint* breakpoint = file->find(ar->currentline);
            if (breakpoint != nullptr && takeBreakpoint(L, *breakpoint)) {
                halt({.thread = L, .reason = Reason::Breakpoint, .breakpoints = {breakpoint->id}});
                return;
            }
        }
    }
    if (step != Step::None && isStepDone(L)) {
        halt({.thread = L, .reason = step == Step::Pause ? Reason::Pause : Reason::Step});
    }
}

// A step in goes on into a coroutine that the stepping coroutine resumes, which then holds frames of its own without running.
bool Tracer::isStepDone(lua_State* L) {
    switch (step) {
    case Step::Pause:
        return true;
    case Step::In: {
        lua_Debug level{};
        return L == stepThread || (stepThread != nullptr && lua_status(stepThread) == LUA_OK && lua_getstack(stepThread, 0, &level) != 0);
    }
    case Step::Over:
        return L == stepThread && getDepth(L) <= stepDepth;
    case Step::Out:
        return L == stepThread && getDepth(L) < stepDepth;
    case Step::None:
        return false;
    }
    return false;
}

int Tracer::getDepth(lua_State* L) {
    lua_Debug level{};
    int depth = 0;
    while (lua_getstack(L, depth, &level) != 0) {
        ++depth;
    }
    return depth;
}

// The condition and the message of a log point run in the frame of the line, and a condition that fails halts with its error, so the person sees why.
bool Tracer::takeBreakpoint(lua_State* L, Breakpoints::Breakpoint& breakpoint) {
    if (!breakpoint.condition.empty()) {
        const Busy guard(*this);
        HaltedStack stack(L, 0);
        bool holds = false;
        if (stack.pushEnvironment(1)) {
            const Evaluation::Result result = Evaluation::run(L, -1, breakpoint.condition);
            if (!result.ok) {
                debugger.output("The condition \"" + breakpoint.condition + "\" of the breakpoint at line " + std::to_string(breakpoint.line) + " failed: " + result.error + "\n");
                holds = true;
            } else {
                holds = result.count > 0 && lua_toboolean(L, -result.count) != 0;
                lua_pop(L, result.count);
            }
            lua_pop(L, 1);
        }
        if (!holds) {
            return false;
        }
    }

    ++breakpoint.hits;
    if (!breakpoint.hit.passes(breakpoint.hits)) {
        return false;
    }
    if (breakpoint.logMessage.empty()) {
        return true;
    }
    const Busy guard(*this);
    debugger.output(interpolate(L, breakpoint.logMessage) + "\n");
    return false;
}

// Every expression between braces turns into the text of its first value, or of its error.
std::string Tracer::interpolate(lua_State* L, const std::string& message) {
    HaltedStack stack(L, 0);
    if (!stack.pushEnvironment(1)) {
        return message;
    }
    const int environment = lua_gettop(L);
    std::string text;
    std::size_t position = 0;
    while (position < message.size()) {
        const std::size_t open = message.find('{', position);
        const std::size_t close = open == std::string::npos ? std::string::npos : message.find('}', open);
        if (close == std::string::npos) {
            text += message.substr(position);
            break;
        }
        text += message.substr(position, open - position);
        const Evaluation::Result result = Evaluation::run(L, environment, std::string_view(message).substr(open + 1, close - open - 1));
        if (!result.ok) {
            text += "(" + result.error + ")";
        } else {
            text += result.count > 0 ? HaltedStack::formatValue(L, -result.count) : "nil";
            lua_pop(L, result.count);
        }
        position = close + 1;
    }
    lua_settop(L, environment - 1);
    return text;
}

// The coroutine takes the hooks it needs once the app goes on, which a step or a change of the breakpoints during the halt may have changed.
void Tracer::halt(Halt value) {
    if (busy || suspended) {
        return;
    }
    lua_State* thread = value.thread;
    {
        const Busy guard(*this);
        debugger.halt(value);
    }
    refresh(thread);
}

void Tracer::haltHere(lua_State* L, int level, std::string text) {
    halt({.thread = L, .level = level, .reason = Reason::Pause, .text = std::move(text)});
}

} // namespace haylen::debug
