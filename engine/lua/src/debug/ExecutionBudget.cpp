#include "debug/ExecutionBudget.hpp"

#include <algorithm>
#include <exception>
#include <utility>

#include "debug/ThreadWalk.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/lua/Runtime.hpp"
#include "runtime/ErrorReader.hpp"

namespace haylen::lua::debug {

void ExecutionBudget::enter(std::string_view stage) noexcept {
    if (entries++ > 0) {
        return;
    }

    deadline = Clock::now() + limits.time;
    entryStage = stage;
}

void ExecutionBudget::leave() noexcept {
    if (--entries > 0) {
        return;
    }

    deadline = Clock::time_point::max();

    if (memoryExhausted && !exhausted) {
        cancelForMemory();
    }
}

ExecutionBudget::~ExecutionBudget() {
    detach();
}

// The coroutines that exist already keep the hooks they have, such as those of the tracer of the debugger, and add the count of the budget.
void ExecutionBudget::attach(lua_State* L, Limits value, Cancel cancel, StageReader stage) {
    state = L;
    limits = value;
    cancelApp = std::move(cancel);
    readStage = std::move(stage);
    exhausted = false;
    memoryExhausted = false;
    refusal.reset();
    message.clear();
    current = this;

    if (entries > 0) {
        deadline = Clock::now() + limits.time;
    }

    used = static_cast<std::size_t>(lua_gc(L, LUA_GCCOUNT)) * 1024 + static_cast<std::size_t>(lua_gc(L, LUA_GCCOUNTB));
    baseAllocator = lua_getallocf(L, &baseData);
    lua_setallocf(L, &allocate, this);

    lua_State* main = lua::Runtime::getMainThread(L);
    watch(main);

    if (L != main) {
        watch(L);
    }

    ThreadWalk::run(L, &watch);
}

void ExecutionBudget::detach() noexcept {
    if (current == this) {
        current = nullptr;
    }

    state = nullptr;
    cancelApp = nullptr;
    readStage = nullptr;
    exhausted = false;
}

// The code that stops a cancelled app runs with a whole budget again, so it can end what the app holds, and the memory it may take stays bounded.
void ExecutionBudget::finishApp() noexcept {
    cancelApp = nullptr;
    readStage = nullptr;
    exhausted = false;
    memoryExhausted = false;
    refusal.reset();
    limits.memory = std::max(limits.memory, used + limits.memory / kStopMemoryShare);

    if (entries > 0) {
        deadline = Clock::now() + limits.time;
    }
}

// Lua passes the kind of a new object in place of its old size, so only a block that exists has a size to give back. A refusal runs the budget out unless the very next growth is its retry and succeeds, since only the emergency collection of Lua comes between a refusal and its retry, while a refusal that Lua raised as an error stays, whatever the app allocates after catching it.
void* ExecutionBudget::allocate(void* data, void* block, std::size_t oldSize, std::size_t newSize) {
    auto& budget = *static_cast<ExecutionBudget*>(data);
    const std::size_t previous = block != nullptr ? oldSize : 0;
    const bool grows = newSize > previous;
    const Refusal asked{.block = block, .oldSize = oldSize, .newSize = newSize};
    const bool retry = grows && budget.refusal == asked;
    if (grows) {
        budget.refusal.reset();
    }

    if (grows && !budget.walking && budget.used - previous + newSize > budget.limits.memory) {
        if (budget.cancelApp && !budget.memoryExhausted) {
            budget.memoryExhausted = true;
            budget.memoryStage = budget.readCurrentStage();
            budget.refusal = asked;
        }

        return nullptr;
    }

    void* result = budget.baseAllocator(budget.baseData, block, oldSize, newSize);
    if (result == nullptr && newSize > 0) {
        return nullptr;
    }

    if (retry && !budget.exhausted) {
        budget.memoryExhausted = false;
    }

    budget.used = budget.used - previous + newSize;
    return result;
}

int ExecutionBudget::getMask() noexcept {
    return current != nullptr ? LUA_MASKCOUNT : 0;
}

int ExecutionBudget::getCount() noexcept {
    if (current == nullptr) {
        return 0;
    }

    return current->exhausted ? 1 : kCheckInstructions;
}

void ExecutionBudget::hook(lua_State* L) {
    if (current == nullptr) {
        lua_sethook(L, nullptr, 0, 0);
        return;
    }

    lua_sethook(L, &onHook, LUA_MASKCOUNT, getCount());
}

void ExecutionBudget::watch(lua_State* thread) {
    lua_Hook existing = lua_gethook(thread);
    if (existing == nullptr) {
        hook(thread);
        return;
    }

    lua_sethook(thread, existing, lua_gethookmask(thread) | LUA_MASKCOUNT, getCount());
}

// The walk takes its memory outside the budget, since that memory belongs to the engine and goes away when the walk ends, so a budget whose memory ran out still reaches every coroutine.
void ExecutionBudget::watchEveryInstruction(lua_State* L) noexcept {
    walking = true;
    lua_pushcfunction(L, &watchCoroutines);
    const int status = lua_pcall(L, 0, 0, 0);
    walking = false;

    if (status != LUA_OK) {
        haylen::core::Log::error("The coroutines of the app whose budget ran out could not all be stopped: {}", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
}

int ExecutionBudget::watchCoroutines(lua_State* L) {
    watch(lua::Runtime::getMainThread(L));
    ThreadWalk::run(L, &watch);
    return 0;
}

void ExecutionBudget::onHook(lua_State* L, lua_Debug* ar) {
    if (ar->event == LUA_HOOKCOUNT) {
        check(L);
    }
}

// The coroutine that raises looks at the budget on every instruction from then on, so code that catches the error raises it again before it does anything else.
void ExecutionBudget::check(lua_State* L) {
    ExecutionBudget* budget = current;
    if (budget == nullptr) {
        return;
    }

    if (!budget->exhausted && budget->memoryExhausted) {
        budget->exhaust(L, budget->describeMemory(), haylen::core::AppError::Code::Memory);
    }

    if (!budget->exhausted) {
        if (Clock::now() < budget->deadline) {
            return;
        }

        budget->exhaust(L, "ran for more than " + std::to_string(budget->limits.time.count()) + " ms", haylen::core::AppError::Code::ExecutionBudget);
    }

    lua_sethook(L, lua_gethook(L), lua_gethookmask(L), 1);
    lua_pushstring(L, budget->message.c_str());
    lua_error(L);
}

void ExecutionBudget::suspend() noexcept {
    if (current != nullptr) {
        current->deadline = Clock::time_point::max();
    }
}

void ExecutionBudget::renew() noexcept {
    ExecutionBudget* budget = current;
    if (budget != nullptr && budget->entries > 0) {
        budget->deadline = Clock::now() + budget->limits.time;
    }
}

void ExecutionBudget::guardMessageHandlers(lua_State* L) {
    lua_getglobal(L, "xpcall");
    lua_pushcclosure(L, &callGuarded, 1);
    lua_setglobal(L, "xpcall");
}

// Calls the original `xpcall`, held as the upvalue, with the handler of the app wrapped, and lets the function it calls yield, as the original does.
int ExecutionBudget::callGuarded(lua_State* L) {
    luaL_checkany(L, 2);
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_insert(L, 1);
    lua_pushvalue(L, 3);
    lua_pushcclosure(L, &handleGuarded, 1);
    lua_replace(L, 3);
    lua_callk(L, lua_gettop(L) - 1, LUA_MULTRET, 0, &finishGuarded);
    return lua_gettop(L);
}

int ExecutionBudget::finishGuarded(lua_State* L, int, lua_KContext) {
    return lua_gettop(L);
}

// The handler of the app, held as the upvalue, sees every error but the one of a budget that ran out, which goes on as it is.
int ExecutionBudget::handleGuarded(lua_State* L) {
    if (current != nullptr && current->exhausted) {
        lua_settop(L, 1);
        return 1;
    }

    lua_pushvalue(L, lua_upvalueindex(1));
    lua_insert(L, 1);
    lua_call(L, lua_gettop(L) - 1, 1);
    return 1;
}

void ExecutionBudget::guardFinalizers(lua_State* L) {
    lua_pushglobaltable(L);
    guardMetatableFunction(L, -1, "setmetatable");
    lua_getglobal(L, "debug");
    guardMetatableFunction(L, -1, "setmetatable");
    lua_pop(L, 2);
}

// Replaces the function of the table at index with a guarded one that holds it as its upvalue.
void ExecutionBudget::guardMetatableFunction(lua_State* L, int table, const char* name) {
    const int library = lua_absindex(L, table);
    lua_getfield(L, library, name);
    lua_pushcclosure(L, &setGuardedMetatable, 1);
    lua_setfield(L, library, name);
}

// Sets the metatable with the original function, held as the upvalue, and then wraps the finalizer that the metatable holds, so a metatable that the original refuses stays as it was. A finalizer wrapped before stays as it is.
int ExecutionBudget::setGuardedMetatable(lua_State* L) {
    const int arguments = lua_gettop(L);
    lua_pushvalue(L, lua_upvalueindex(1));
    for (int index = 1; index <= arguments; ++index) {
        lua_pushvalue(L, index);
    }

    lua_call(L, arguments, 1);

    if (!lua_istable(L, 2)) {
        return 1;
    }

    lua_pushliteral(L, "__gc");
    lua_rawget(L, 2);
    const bool wrapped = lua_tocfunction(L, -1) == &runFinalizer;
    if (!lua_isfunction(L, -1) || wrapped) {
        lua_pop(L, 1);
        return 1;
    }

    lua_pushcclosure(L, &runFinalizer, 1);
    lua_pushliteral(L, "__gc");
    lua_insert(L, -2);
    lua_rawset(L, 2);
    return 1;
}

// Runs the finalizer of the app, held as the upvalue, in a new coroutine, which takes the hooks of the thread that collects. An error of the finalizer, also the one of a budget that ran out, goes on to Lua, which reports it as a warning of the finalizer.
int ExecutionBudget::runFinalizer(lua_State* L) {
    lua_State* thread = lua_newthread(L);
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_pushvalue(L, 1);
    lua_xmove(L, thread, 2);

    int results = 0;
    const int status = lua_resume(thread, L, 1, &results);
    if (status == LUA_OK) {
        return 0;
    }

    if (status == LUA_YIELD) {
        return luaL_error(L, "A finalizer cannot yield, since Lua runs it while it collects garbage.");
    }

    lua_closethread(thread, L);
    lua_xmove(thread, L, 1);
    return lua_error(L);
}

std::string ExecutionBudget::readCurrentStage() const {
    std::string stage = readStage ? std::string(readStage()) : std::string();
    return stage.empty() ? std::string(entryStage) : stage;
}

std::string ExecutionBudget::describeMemory() const {
    return "took more than " + std::to_string(limits.memory / (1024 * 1024)) + " MB of Lua memory";
}

std::string ExecutionBudget::describe(const std::string& reason, const std::string& stage) {
    return "The code of the app " + reason + " in \"" + stage + "\", so the app stopped. Fix the code and run the app again.";
}

// No code of the app runs at this point, so the error names the step where the allocation failed and has no stack of its own.
void ExecutionBudget::cancelForMemory() noexcept {
    exhausted = true;

    try {
        message = describe(describeMemory(), memoryStage);
        haylen::core::AppError error = lua::ErrorReader::read(message);
        error.setCode(haylen::core::AppError::Code::Memory);
        error.setCallback(memoryStage);
        cancelApp(error);
    } catch (const std::exception& failure) {
        haylen::core::Log::error("The app that ran out of memory could not be cancelled: {}", failure.what());
    }

    watchEveryInstruction(state);
}

void ExecutionBudget::exhaust(lua_State* L, const std::string& reason, haylen::core::AppError::Code code) {
    exhausted = true;
    const std::string stage = code == haylen::core::AppError::Code::Memory ? memoryStage : readCurrentStage();
    message = describe(reason, stage);

    if (!cancelApp) {
        return;
    }

    haylen::core::AppError error = lua::Runtime::captureError(L, message, 0);
    error.setCode(code);
    error.setCallback(stage);

    try {
        cancelApp(error);
    } catch (const std::exception& failure) {
        haylen::core::Log::error("The app whose code ran too long could not be cancelled: {}", failure.what());
    }

    watchEveryInstruction(L);
}

} // namespace haylen::lua::debug
