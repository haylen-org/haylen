#include "plugins/JobsPlugin.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include "haylen/lua/Runtime.hpp"
#include "lua/JobsLua.hpp"
#include "lua/Task.hpp"

namespace haylen::plugins {

void JobsPlugin::stop(core::Engine&) {
    jobs.clear();
}

void JobsPlugin::installLua(core::Engine&, lua_State* L) {
    lua::JobsLua::install(L);
}

void JobsPlugin::spawn(lua_State* L, int arguments, lua::Promise promise) {
    lua_State* thread = lua_newthread(L);
    lua::Reference reference(L, -1);
    lua_pop(L, 1);

    // The function and its arguments are the last values on the stack, and the new coroutine starts with copies of them, so both stacks need room for all of them.
    if (lua_checkstack(L, arguments + 1) == 0 || lua_checkstack(thread, arguments + 1) == 0) {
        throw std::runtime_error("A job cannot start with that many arguments.");
    }
    const int top = lua_gettop(L);
    for (int index = top - arguments; index <= top; ++index) {
        lua_pushvalue(L, index);
    }
    lua_xmove(L, thread, arguments + 1);
    jobs.push_back({.thread = std::move(reference), .state = thread, .promise = std::move(promise), .arguments = arguments});
}

void JobsPlugin::setBudget(std::chrono::microseconds value) {
    if (value.count() <= 0) {
        throw std::invalid_argument("The job budget must be a positive number of milliseconds.");
    }
    budget = value;
}

bool JobsPlugin::isOutOfTime(lua_State* L) const {
    if (L != current) {
        throw std::logic_error("A jobs.checkpoint call only runs inside a job started with jobs.spawn.");
    }
    return std::chrono::steady_clock::now() >= deadline;
}

// Jobs take turns in order, so a job that used the whole budget goes last on the next frame.
void JobsPlugin::update(core::Engine&, float) {
    const auto now = std::chrono::steady_clock::now();
    const auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::time_point::max() - now);
    deadline = budget >= remaining ? std::chrono::steady_clock::time_point::max() : now + budget;
    const std::size_t turns = jobs.size();
    for (std::size_t turn = 0; turn < turns && std::chrono::steady_clock::now() < deadline; ++turn) {
        Job job = std::move(jobs.front());
        jobs.pop_front();
        if (resume(job)) {
            jobs.push_back(std::move(job));
        }
    }
}

bool JobsPlugin::resume(Job& job) {
    lua_State* main = job.thread.getState();
    int results = 0;
    current = job.state;
    const int status = lua_resume(job.state, main, std::exchange(job.arguments, 0), &results);
    current = nullptr;

    // A job that waits for anything else ends there, so a promise it waits for never resumes its code.
    if (status == LUA_YIELD) {
        const bool paused = lua::JobsLua::isCheckpoint(job.state, results);
        lua_pop(job.state, results);
        if (paused) {
            return true;
        }
        const std::optional<lua::Error> failure = lua::Task::closeCoroutine(job.state, main);
        job.promise.reject("A job may only pause at jobs.checkpoint. Wait for promises inside async.spawn instead.");
        if (failure) {
            lua::Runtime::reportError(main, *failure);
        }
        return false;
    }

    // The failed coroutine keeps its stack, so the rejection carries the stack of the job without the tab characters of a Lua traceback.
    if (status != LUA_OK) {
        const char* message = lua_tostring(job.state, -1);
        const lua::Error error = lua::Runtime::captureError(job.state, message != nullptr ? message : "The job failed with an error that is not a string.", 0);
        job.promise.reject(std::string(error.what()) + "\n" + error.getTraceback());
        return false;
    }

    if (results == 0) {
        job.promise.resolveWith([](lua_State* state) { lua_pushnil(state); });
        return false;
    }
    auto value = std::make_shared<lua::Reference>(job.state, -results);
    lua_pop(job.state, results);
    job.promise.resolveWith([value](lua_State* state) { value->push(state); });
    return false;
}

} // namespace haylen::plugins
