#include "debug/Evaluation.hpp"

namespace haylen::debug {

// The coroutine of the evaluation stands on the stack of `L` until its results moved over, and an evaluation that yields is closed, since nothing resumes it.
Evaluation::Result Evaluation::run(lua_State* L, int environment, std::string_view source, std::chrono::milliseconds limit) {
    const int env = lua_absindex(L, environment);
    lua_State* thread = lua_newthread(L);
    const int threadIndex = lua_gettop(L);

    const std::string expression = "return " + std::string(source);
    if (luaL_loadbufferx(thread, expression.data(), expression.size(), "=(evaluation)", "t") != LUA_OK) {
        lua_pop(thread, 1);
        if (luaL_loadbufferx(thread, source.data(), source.size(), "=(evaluation)", "t") != LUA_OK) {
            Result failed{.error = describeError(thread, -1)};
            lua_settop(L, threadIndex - 1);
            return failed;
        }
    }
    lua_pushvalue(L, env);
    lua_xmove(L, thread, 1);
    (void)lua_setupvalue(thread, -2, 1);

    deadline = std::chrono::steady_clock::now() + limit;
    allowed = limit;
    appHook = lua_gethook(L);
    appMask = lua_gethookmask(L);
    appCount = lua_gethookcount(L);
    lua_sethook(thread, &onCount, LUA_MASKCOUNT, kCheckInstructions);
    running = true;
    int results = 0;
    const int status = lua_resume(thread, L, 0, &results);
    running = false;
    if (status == LUA_YIELD) {
        (void)lua_closethread(thread, L);
        lua_settop(L, threadIndex - 1);
        return {.error = "The evaluation tried to wait, which it cannot do while the app halts."};
    }
    if (status != LUA_OK) {
        Result failed{.error = describeError(thread, -1)};
        lua_settop(L, threadIndex - 1);
        return failed;
    }

    lua_xmove(thread, L, results);
    lua_remove(L, threadIndex);
    return {.ok = true, .count = results};
}

std::string Evaluation::describeError(lua_State* L, int index) {
    if (lua_type(L, index) == LUA_TSTRING || lua_type(L, index) == LUA_TNUMBER) {
        lua_pushvalue(L, index);
        std::string text = lua_tostring(L, -1);
        lua_pop(L, 1);
        return text;
    }
    return std::string("(error object is a ") + luaL_typename(L, index) + " value)";
}

// A coroutine that the evaluation created, such as a task that it spawned, inherited its hook and runs on after it as code of the app, so it takes the hook that the app had instead of the time bound of the evaluation.
void Evaluation::onCount(lua_State* L, lua_Debug*) {
    if (!running) {
        lua_sethook(L, appHook, appMask, appCount);
        return;
    }
    if (std::chrono::steady_clock::now() > deadline) {
        luaL_error(L, "The evaluation ran longer than %d ms, so it stopped.", static_cast<int>(allowed.count()));
    }
}

} // namespace haylen::debug
