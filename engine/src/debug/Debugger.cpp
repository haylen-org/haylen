#include "debug/Debugger.hpp"

#include <algorithm>
#include <thread>
#include <utility>
#include <vector>

#include "debug/Breakpoints.hpp"
#include "debug/Evaluation.hpp"
#include "debug/HaltedStack.hpp"
#include "haylen/core/Log.hpp"
#include "platform/Host.hpp"

namespace haylen::debug {

Debugger::Debugger(platform::Host& platformHost, Send sender, Pump pumper) : host(platformHost), send(std::move(sender)), pump(std::move(pumper)) {}

Debugger::~Debugger() = default;

Debugger* Debugger::find(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, kRegistryKey);
    auto* debugger = static_cast<Debugger*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return debugger;
}

void Debugger::attach(lua_State* L) {
    state = L;
    lua_pushlightuserdata(L, this);
    lua_setfield(L, LUA_REGISTRYINDEX, kRegistryKey);
    tracer.attach(L);
}

void Debugger::detach() noexcept {
    tracer.detach();
    state = nullptr;
}

// A message that is no request of the protocol reaches nothing, since the client only ever sends requests.
void Debugger::receive(core::Json message) {
    if (!message.is_object() || message.value("type", "") != "request" || !message.contains("command") || !message.at("command").is_string()) {
        core::Log::warning("The debugger received a message that is no request of the Debug Adapter Protocol.");
        return;
    }
    inbox.push_back(std::move(message));
}

void Debugger::update(lua_State* L) {
    tracer.beginFrame();
    while (!inbox.empty()) {
        const core::Json request = std::move(inbox.front());
        inbox.pop_front();
        handle(request, L);
    }
}

void Debugger::reset() {
    attached = false;
    tracer.getBreakpoints().clear();
    tracer.setErrorsWatched(false);
    tracer.setStep(Tracer::Step::None);
    inbox.clear();
    if (state != nullptr) {
        tracer.refresh(state);
    }
    if (isHalted()) {
        next = Resume::Continue;
    }
}

void Debugger::output(const std::string& text) {
    sendEvent("output", {{"category", "console"}, {"output", text}});
}

void Debugger::breakHere(lua_State* L) {
    if (attached) {
        tracer.haltHere(L, 1, "The app called \"debug.breakpoint\".");
    }
}

// The collector holds while the app halts, so no finalizer of the app runs while the person looks at its values. The platform may ask the frame to finish, such as when the person quits, and then the app goes on without halting until the next frame.
void Debugger::halt(const Tracer::Halt& value) {
    if (!attached || state == nullptr || !host.canHaltFrames()) {
        return;
    }
    const bool collecting = lua_gc(value.thread, LUA_GCISRUNNING) != 0;
    lua_gc(value.thread, LUA_GCSTOP);
    halted = value;
    stack = std::make_unique<HaltedStack>(value.thread, value.level);
    next = Resume::None;

    core::Json body{{"reason", Tracer::kReasonNames[static_cast<std::size_t>(value.reason)]}, {"threadId", kThreadId}, {"allThreadsStopped", true}};
    if (!value.text.empty()) {
        body["text"] = value.text;
        body["description"] = value.text;
    }
    if (!value.breakpoints.empty()) {
        body["hitBreakpointIds"] = value.breakpoints;
    }
    sendEvent("stopped", std::move(body));

    bool released = false;
    while (next == Resume::None) {
        if (host.pumpHaltedEvents()) {
            tracer.suspendUntilFrame();
            output("The app goes on to the end of its frame without halting, since the platform asked it to finish.\n");
            next = Resume::Continue;
            released = true;
            break;
        }
        pump();
        while (!inbox.empty() && next == Resume::None) {
            const core::Json request = std::move(inbox.front());
            inbox.pop_front();
            handle(request, value.thread);
        }
        if (next == Resume::None && inbox.empty()) {
            std::this_thread::sleep_for(kHaltPoll);
        }
    }
    released = released || !attached;

    stack.reset();
    if (collecting) {
        lua_gc(value.thread, LUA_GCRESTART);
    }

    // The stack of an error unwinds once the app goes on, so a step after an error lets it go on.
    const bool steps = next != Resume::Continue && value.reason != Tracer::Reason::Error;
    if (!steps) {
        tracer.setStep(Tracer::Step::None);
    } else {
        tracer.setStep(next == Resume::In ? Tracer::Step::In : next == Resume::Over ? Tracer::Step::Over : Tracer::Step::Out, value.thread, value.level);
    }
    if (released && attached) {
        sendEvent("continued", {{"threadId", kThreadId}, {"allThreadsContinued", true}});
    }
}

void Debugger::handle(const core::Json& request, lua_State* L) {
    const std::string command = request.at("command").get<std::string>();
    if (command == "initialize") {
        respond(request, getCapabilities());
        sendEvent("initialized");
    } else if (command == "attach") {
        attached = true;
        respond(request);
    } else if (command == "launch") {
        fail(request, "The debugger attaches to an app that runs, which \"haylen run\" starts. Use a configuration whose request is \"attach\".");
    } else if (command == "configurationDone") {
        respond(request);
    } else if (command == "disconnect") {
        reset();
        respond(request);
    } else if (command == "setBreakpoints") {
        setBreakpoints(request, L);
    } else if (command == "setExceptionBreakpoints") {
        setExceptionBreakpoints(request, L);
    } else if (command == "threads") {
        respond(request, {{"threads", {{{"id", kThreadId}, {"name", "Lua"}}}}});
    } else if (command == "stackTrace") {
        if (!isHalted()) {
            fail(request, "The app runs, so it has no stack to show. Pause it first.");
            return;
        }
        respond(request, stack->getFrames(readInt(request, "startFrame", 0), readInt(request, "levels", 0)));
    } else if (command == "scopes" || command == "variables") {
        core::Json body;
        if (isHalted()) {
            body = command == "scopes" ? stack->getScopes(readInt(request, "frameId", 0)) : stack->getVariables(readInt(request, "variablesReference", 0), readInt(request, "start", 0), readInt(request, "count", 0));
        }
        if (body.is_null()) {
            fail(request, isHalted() ? "The frame or the variable no longer exists." : "The app runs, so it has no values to show. Pause it first.");
            return;
        }
        respond(request, std::move(body));
    } else if (command == "evaluate") {
        evaluate(request, L);
    } else if (command == "continue") {
        resume(request, Resume::Continue);
    } else if (command == "next") {
        resume(request, Resume::Over);
    } else if (command == "stepIn") {
        resume(request, Resume::In);
    } else if (command == "stepOut") {
        resume(request, Resume::Out);
    } else if (command == "pause") {
        pause(request, L);
    } else {
        fail(request, "The debugger does not support the request \"" + command + "\".");
    }
}

// A page of the web runs the app on the thread that delivers every message, so it cannot halt, and only log points and the errors it reports work there.
core::Json Debugger::getCapabilities() const {
    const bool halts = host.canHaltFrames();
    core::Json filters = core::Json::array();
    if (halts) {
        filters.push_back({{"filter", kErrorFilter}, {"label", "Uncaught errors"}, {"description", "Halts where an error rises that no pcall or xpcall of the app catches, before its stack unwinds."}, {"default", true}});
    }
    return {{"supportsConfigurationDoneRequest", true}, {"supportsConditionalBreakpoints", true}, {"supportsHitConditionalBreakpoints", true}, {"supportsLogPoints", true}, {"supportsEvaluateForHovers", true}, {"exceptionBreakpointFilters", std::move(filters)}};
}

// The development server turns the paths of the client into package paths, such as `source/main.lua`, which name the chunks of the app.
void Debugger::setBreakpoints(const core::Json& request, lua_State* L) {
    const core::Json arguments = request.value("arguments", core::Json::object());
    const core::Json source = arguments.value("source", core::Json::object());
    const std::string path = source.value("path", "");
    if (path.empty()) {
        fail(request, "The breakpoints name no source path.");
        return;
    }

    const bool halts = host.canHaltFrames();
    std::vector<Breakpoints::Request> requests;
    std::vector<std::size_t> kept;
    const core::Json asked = arguments.value("breakpoints", core::Json::array());
    for (std::size_t index = 0; index < asked.size(); ++index) {
        const core::Json& item = asked[index];
        Breakpoints::Request entry{.line = item.value("line", 0), .condition = item.value("condition", ""), .hitCondition = item.value("hitCondition", ""), .logMessage = item.value("logMessage", "")};
        if (!halts && entry.logMessage.empty()) {
            continue;
        }
        kept.push_back(index);
        requests.push_back(std::move(entry));
    }

    const std::vector<Breakpoints::Result> results = tracer.getBreakpoints().set(path, requests);
    core::Json breakpoints = core::Json::array();
    std::size_t position = 0;
    for (std::size_t index = 0; index < asked.size(); ++index) {
        if (position < kept.size() && kept[position] == index) {
            const Breakpoints::Result& result = results[position++];
            core::Json item{{"id", result.id}, {"line", result.line}, {"verified", result.verified}};
            if (!result.message.empty()) {
                item["message"] = result.message;
            }
            breakpoints.push_back(std::move(item));
            continue;
        }
        breakpoints.push_back({{"verified", false}, {"line", asked[index].value("line", 0)}, {"message", "A web page cannot halt, since it runs the app on the thread that delivers every message, so only log points work there."}});
    }
    if (state != nullptr) {
        tracer.refresh(L);
    }
    respond(request, {{"breakpoints", std::move(breakpoints)}});
}

void Debugger::setExceptionBreakpoints(const core::Json& request, lua_State* L) {
    const core::Json filters = request.value("arguments", core::Json::object()).value("filters", core::Json::array());
    tracer.setErrorsWatched(host.canHaltFrames() && std::find(filters.begin(), filters.end(), core::Json(kErrorFilter)) != filters.end());
    if (state != nullptr) {
        tracer.refresh(L);
    }
    respond(request);
}

// An evaluation while the app runs sees the globals, and one in a frame of a halt sees that frame. Values that hold more values open in the client only during a halt, whose view keeps them.
void Debugger::evaluate(const core::Json& request, lua_State* L) {
    if (state == nullptr) {
        fail(request, "No app runs, so nothing can be evaluated.");
        return;
    }
    const std::string expression = readText(request, "expression");
    const int top = lua_gettop(L);
    const int frameId = readInt(request, "frameId", 0);
    if (isHalted() && frameId > 0) {
        if (!stack->pushEnvironment(frameId)) {
            fail(request, "The frame no longer exists.");
            return;
        }
    } else {
        lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
    }

    const Evaluation::Result result = Evaluation::run(L, -1, expression);
    if (!result.ok) {
        lua_settop(L, top);
        fail(request, result.error);
        return;
    }
    core::Json body{{"result", ""}, {"variablesReference", 0}};
    for (int index = 0; index < result.count; ++index) {
        const int value = lua_gettop(L) - result.count + 1 + index;
        if (result.count == 1 && isHalted()) {
            const core::Json variable = stack->describe("", value);
            body = {{"result", variable.at("value")}, {"type", variable.at("type")}, {"variablesReference", variable.at("variablesReference")}};
            break;
        }
        body["result"] = body.at("result").get<std::string>() + (index > 0 ? ", " : "") + HaltedStack::formatValue(L, value);
    }
    lua_settop(L, top);
    respond(request, std::move(body));
}

void Debugger::resume(const core::Json& request, Resume value) {
    if (!isHalted()) {
        if (value == Resume::Continue) {
            respond(request, {{"allThreadsContinued", true}});
            return;
        }
        fail(request, "The app runs, so it cannot step. Pause it first.");
        return;
    }
    next = value;
    respond(request, value == Resume::Continue ? core::Json{{"allThreadsContinued", true}} : core::Json::object());
}

// A pause halts at the next line of the app that runs, in any coroutine.
void Debugger::pause(const core::Json& request, lua_State* L) {
    if (!host.canHaltFrames()) {
        fail(request, "A web page cannot halt, since it runs the app on the thread that delivers every message.");
        return;
    }
    if (!isHalted()) {
        tracer.setStep(Tracer::Step::Pause);
        if (state != nullptr) {
            tracer.refresh(L);
        }
    }
    respond(request);
}

void Debugger::respond(const core::Json& request, core::Json body) {
    send({{"seq", ++sequence}, {"type", "response"}, {"request_seq", request.value("seq", 0)}, {"success", true}, {"command", request.at("command")}, {"body", std::move(body)}});
}

void Debugger::fail(const core::Json& request, const std::string& message) {
    send({{"seq", ++sequence}, {"type", "response"}, {"request_seq", request.value("seq", 0)}, {"success", false}, {"command", request.at("command")}, {"message", message}, {"body", {{"error", {{"id", 1}, {"format", message}}}}}});
}

void Debugger::sendEvent(const std::string& event, core::Json body) {
    send({{"seq", ++sequence}, {"type", "event"}, {"event", event}, {"body", std::move(body)}});
}

int Debugger::readInt(const core::Json& request, const char* key, int fallback) {
    const core::Json arguments = request.value("arguments", core::Json::object());
    const auto found = arguments.find(key);
    return found != arguments.end() && found->is_number_integer() ? found->get<int>() : fallback;
}

std::string Debugger::readText(const core::Json& request, const char* key) {
    const core::Json arguments = request.value("arguments", core::Json::object());
    const auto found = arguments.find(key);
    return found != arguments.end() && found->is_string() ? found->get<std::string>() : std::string();
}

} // namespace haylen::debug
