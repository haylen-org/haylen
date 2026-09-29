#include "lua/Task.hpp"

#include <exception>
#include <new>
#include <utility>

#include "haylen/core/Engine.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/lua/Promise.hpp"
#include "haylen/lua/Runtime.hpp"
#include "lua/Owners.hpp"

namespace haylen::lua {

Task::Task(lua_State* L, Completion onFinish) : main(Runtime::getMainThread(L)), completion(std::move(onFinish)) {}

// A task that still holds its coroutine when it is destroyed only goes away with the Lua state, which drops the coroutine itself.
Task::~Task() = default;

std::shared_ptr<Task> Task::start(lua_State* L, int function, int arguments, int owner, Completion onFinish) {
    luaL_checktype(L, function, LUA_TFUNCTION);
    auto task = std::make_shared<Task>(L, std::move(onFinish));
    if (owner != 0) {
        Owners::addTask(L, owner, task);
    }
    task->run(L, function, arguments);
    return task;
}

void Task::run(lua_State* L, int function, int arguments) {
    const int first = lua_absindex(L, function);
    thread = lua_newthread(L);
    threadReference = luaL_ref(L, LUA_REGISTRYINDEX);

    // The body holds the task, so the task lives for as long as its coroutine may still run.
    if (luaL_newmetatable(thread, kHolderType) != 0) {
        lua_pushcfunction(thread, &collectHolder);
        lua_setfield(thread, -2, "__gc");
    }
    lua_pop(thread, 1);
    new (lua_newuserdatauv(thread, sizeof(std::shared_ptr<Task>), 0)) std::shared_ptr<Task>(shared_from_this());
    luaL_setmetatable(thread, kHolderType);
    lua_pushcclosure(thread, &body, 1);

    luaL_checkstack(L, arguments + 1, "too many arguments for a task");
    for (int index = 0; index <= arguments; ++index) {
        lua_pushvalue(L, first + index);
    }
    lua_xmove(L, thread, arguments + 1);

    // The body catches every error of the function, so only a failure of the coroutine itself, such as running out of memory, ends the resume here.
    int results = 0;
    const int status = lua_resume(thread, L, arguments + 1, &results);
    if (status != LUA_OK && status != LUA_YIELD) {
        finish(Runtime::readError(thread, -1));
    }
}

int Task::body(lua_State* L) {
    const int arguments = lua_gettop(L) - 1;
    lua_pushcfunction(L, &Runtime::handleMessage);
    lua_insert(L, 1);
    const int status = lua_pcallk(L, arguments, LUA_MULTRET, 1, kCalled, &continueBody);
    return continueBody(L, status, kCalled);
}

// Runs when the function returned or failed, possibly after waiting, and again once a promise it returned settled.
int Task::continueBody(lua_State* L, int status, lua_KContext context) {
    Task& task = **static_cast<std::shared_ptr<Task>*>(lua_touserdata(L, lua_upvalueindex(1)));
    if (status != LUA_OK && status != LUA_YIELD) {
        task.finish(Runtime::readError(L, -1));
        return 0;
    }
    if (context == kCalled && lua_gettop(L) >= 2 && Promise::isPromise(L, 2)) {
        lua_getfield(L, 2, "await");
        lua_pushvalue(L, 2);
        lua_callk(L, 1, 2, kAwaited, &continueBody);
        return continueBody(L, LUA_OK, kAwaited);
    }
    if (context == kAwaited && !lua_isnil(L, -1)) {
        task.finish(Error(Runtime::describeValue(L, -1)));
        return 0;
    }
    task.finish(std::nullopt);
    return 0;
}

int Task::collectHolder(lua_State* L) {
    auto* holder = static_cast<std::shared_ptr<Task>*>(luaL_testudata(L, 1, kHolderType));
    if (holder == nullptr) {
        return 0;
    }
    holder->~shared_ptr();
    lua_pushnil(L);
    lua_setmetatable(L, 1);
    return 0;
}

int Task::endClosed(lua_State*) {
    return 0;
}

void Task::finish(const std::optional<Error>& error) {
    finished = true;
    release();
    if (cancelled || !completion) {
        return;
    }
    try {
        completion(error);
    } catch (const std::exception& exception) {
        Runtime::reportError(main, exception);
    }
}

void Task::cancel() {
    if (!isRunning()) {
        return;
    }
    cancelled = true;
    if (lua_status(thread) == LUA_YIELD) {
        close();
        return;
    }

    // The coroutine runs right now, so it closes on the event loop, which runs the job once the code waits and before any promise it waits for can resume it.
    // clang-format off
    Runtime::getEngine(main).getJobs().postToFrame([weak = weak_from_this()] {
        if (const std::shared_ptr<Task> task = weak.lock()) {
            task->close();
        }
    });
    // clang-format on
}

// A promise that still waits on the coroutine resumes it later, so the closed coroutine is reset around a function that ends at once, and the resume runs none of the code of the task.
void Task::close() {
    if (finished || threadReference == LUA_NOREF || lua_status(thread) != LUA_YIELD) {
        return;
    }
    const int status = lua_closethread(thread, main);
    std::optional<Error> failure;
    if (status != LUA_OK) {
        failure = Runtime::readError(thread, -1);
    }
    lua_settop(thread, 0);
    lua_pushcfunction(thread, &endClosed);
    release();
    if (failure) {
        Runtime::reportError(main, *failure);
    }
}

void Task::release() noexcept {
    if (threadReference == LUA_NOREF) {
        return;
    }
    luaL_unref(main, LUA_REGISTRYINDEX, threadReference);
    threadReference = LUA_NOREF;
}

} // namespace haylen::lua
