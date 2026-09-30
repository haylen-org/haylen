#include "haylen/lua/Runtime.hpp"

#include <algorithm>
#include <new>
#include <utility>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/Userdata.hpp"
#include "lua/Task.hpp"

namespace haylen::lua {

core::Engine& Runtime::getEngine(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, kEngineKey);
    auto* owner = static_cast<core::Engine*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    if (owner == nullptr) {
        luaL_error(L, "This Lua state is not bound to a Haylen engine.");
    }
    return *owner;
}

lua_State* Runtime::getMainThread(lua_State* L) noexcept {
    lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
    lua_State* main = lua_tothread(L, -1);
    lua_pop(L, 1);
    return main;
}

void Runtime::reportError(lua_State* L, const std::exception& exception) {
    getEngine(L).reportError(exception);
}

void Runtime::runChunk(lua_State* L, std::string_view source, const std::string& chunkName) {
    if (luaL_loadbufferx(L, source.data(), source.size(), chunkName.c_str(), "t") != LUA_OK) {
        Error error = readError(L, -1);
        lua_pop(L, 1);
        throw error;
    }
    protectedCall(L, 0, 0);
}

// Mirrors the stand-alone interpreter: strings and numbers are the message, values with `__tostring` describe themselves and other values name their type.
std::string Runtime::describeValue(lua_State* L, int index) {
    if (lua_isstring(L, index) != 0) {
        return lua_tostring(L, index);
    }
    if (luaL_callmeta(L, index, "__tostring") != 0) {
        std::string text = lua_type(L, -1) == LUA_TSTRING ? lua_tostring(L, -1) : "";
        lua_pop(L, 1);
        if (!text.empty()) {
            return text;
        }
    }
    return std::string("(error object is a ") + luaL_typename(L, index) + " value)";
}

// The protected calls and the failure handler of Varn's tasks build their frames here, so every stack of an error reads the same.
Error::Frame Runtime::makeFrame(const Level& level) {
    return {.source = std::string(level.source), .line = std::max(level.line, 0), .function = describeFunction(level), .kind = getFrameKind(level.what)};
}

Error::Frame Runtime::makeSkippedFrame(int count) {
    return {.source = "...", .function = std::to_string(count) + " levels skipped", .kind = Error::Frame::Kind::C};
}

// Names the function the way Lua tracebacks do, from the calling code when it has a name, and from its kind or definition otherwise.
std::string Runtime::describeFunction(const Level& level) {
    if (!level.nameWhat.empty()) {
        return std::string(level.nameWhat) + " '" + std::string(level.name) + "'";
    }
    if (level.what == "main") {
        return "main chunk";
    }
    if (level.what != "C") {
        return "function <" + std::string(level.source) + ":" + std::to_string(level.lineDefined) + ">";
    }
    return "?";
}

Error::Frame::Kind Runtime::getFrameKind(std::string_view what) noexcept {
    if (what == "C") {
        return Error::Frame::Kind::C;
    }
    return what == "main" ? Error::Frame::Kind::Main : Error::Frame::Kind::Lua;
}

// Finds the outermost level of the stack with a binary search, since reaching a level walks every level above it.
int Runtime::findLastLevel(lua_State* L) {
    lua_Debug info{};
    int low = 1;
    int high = 1;
    while (lua_getstack(L, high, &info) != 0) {
        low = high;
        high *= 2;
    }
    while (low < high) {
        const int middle = low + (high - low) / 2;
        if (lua_getstack(L, middle, &info) != 0) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    return high - 1;
}

Error Runtime::captureError(lua_State* L, const std::string& text, int level) {
    std::vector<Error::Frame> frames;
    lua_Debug info{};
    const int last = findLastLevel(L);
    const bool elide = last - level > kInnerLevels + kOuterLevels;
    for (int current = level; lua_getstack(L, current, &info) != 0; ++current) {
        if (elide && current == level + kInnerLevels) {
            const int skipped = last - kOuterLevels + 1 - current;
            frames.push_back(makeSkippedFrame(skipped));
            current += skipped - 1;
            continue;
        }
        lua_getinfo(L, "Slnf", &info);
        const lua_CFunction function = lua_tocfunction(L, -1);
        lua_pop(L, 1);
        if (function == &runBody) {
            continue;
        }

        // The body of an engine task starts its coroutine, so nothing of the app lies below it.
        if (function == &Task::body) {
            break;
        }

        frames.push_back(makeFrame({.source = info.short_src, .line = info.currentline, .what = info.what, .nameWhat = info.namewhat, .name = info.name != nullptr ? info.name : "", .lineDefined = info.linedefined}));
    }
    return Error(text, std::move(frames));
}

int Runtime::handleMessage(lua_State* L) {
    // clang-format off
    return Binding::guarded(L, [L] {
        pushError(L, captureError(L, describeValue(L, 1), 1));
        return 1;
    });
    // clang-format on
}

// The metatable comes first, so the error is never built inside a userdata that could miss its finalizer.
void Runtime::pushError(lua_State* L, Error error) {
    if (Userdata::newMetatable(L, kErrorType)) {
        lua_pushcfunction(L, &collectError);
        lua_setfield(L, -2, "__gc");
        lua_pushcfunction(L, &errorToString);
        lua_setfield(L, -2, "__tostring");
    }
    void* memory = lua_newuserdatauv(L, sizeof(Error), 0);
    new (memory) Error(std::move(error));
    lua_insert(L, -2);
    lua_setmetatable(L, -2);
}

// Only the collector calls it, and dropping the metatable keeps an error that another finalizer brings back away from the destroyed object.
int Runtime::collectError(lua_State* L) {
    static_cast<Error*>(lua_touserdata(L, 1))->~Error();
    lua_pushnil(L);
    lua_setmetatable(L, 1);
    return 0;
}

int Runtime::errorToString(lua_State* L) {
    lua_pushstring(L, static_cast<const Error*>(luaL_checkudata(L, 1, kErrorType))->what());
    return 1;
}

Error Runtime::readError(lua_State* L, int index) {
    if (const auto* error = static_cast<const Error*>(luaL_testudata(L, index, kErrorType))) {
        return *error;
    }
    return Error(describeValue(L, index));
}

void Runtime::protectedCall(lua_State* L, int arguments, int results) {
    const int function = lua_gettop(L) - arguments;
    lua_pushcfunction(L, &handleMessage);
    lua_insert(L, function);

    const int status = lua_pcall(L, arguments, results, function);
    lua_remove(L, function);
    if (status != LUA_OK) {
        Error error = readError(L, -1);
        lua_pop(L, 1);
        throw error;
    }
}

// The body receives the empty stack it would have as a function of its own.
int Runtime::runBody(lua_State* L) {
    const auto& call = *static_cast<const Call*>(lua_touserdata(L, 1));
    lua_settop(L, 0);
    // clang-format off
    return Binding::guarded(L, [&] {
        call.invoke(call.body, L);
        return 0;
    });
    // clang-format on
}

void Runtime::runProtected(lua_State* L, Call& call) {
    lua_pushcfunction(L, &runBody);
    lua_pushlightuserdata(L, &call);
    protectedCall(L, 1, 0);
}

} // namespace haylen::lua
