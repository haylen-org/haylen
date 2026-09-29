#include "haylen/lua/Runtime.hpp"

#include <new>
#include <utility>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
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

// Mirrors the stand-alone interpreter: strings and numbers are the message, values with __tostring describe themselves and other values name their type.
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

// Names the function the way Lua tracebacks do, from the calling code when it has a name, and from its kind or definition otherwise.
std::string Runtime::describeFunction(const lua_Debug& info) {
    if (*info.namewhat != '\0') {
        return std::string(info.namewhat) + " '" + info.name + "'";
    }
    if (*info.what == 'm') {
        return "main chunk";
    }
    if (*info.what != 'C') {
        return "function <" + std::string(info.short_src) + ":" + std::to_string(info.linedefined) + ">";
    }
    return "?";
}

Error::Frame::Kind Runtime::getFrameKind(const lua_Debug& info) noexcept {
    if (*info.what == 'C') {
        return Error::Frame::Kind::C;
    }
    return *info.what == 'm' ? Error::Frame::Kind::Main : Error::Frame::Kind::Lua;
}

Error Runtime::captureError(lua_State* L, const std::string& text, int level) {
    std::vector<Error::Frame> frames;
    lua_Debug info{};
    for (int current = level; lua_getstack(L, current, &info) != 0; ++current) {
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

        // Only the task wrapper and the coroutine entry of Varn lie below the task chunk, and the native frame right above it is the xpcall that runs the task.
        if (std::string_view(info.source) == kTaskChunk) {
            if (!frames.empty() && frames.back().kind == Error::Frame::Kind::C) {
                frames.pop_back();
            }
            break;
        }

        frames.push_back({.source = info.short_src, .line = info.currentline > 0 ? info.currentline : 0, .function = describeFunction(info), .kind = getFrameKind(info)});
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
    if (luaL_newmetatable(L, kErrorType) != 0) {
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

int Runtime::collectError(lua_State* L) {
    auto* error = static_cast<Error*>(luaL_testudata(L, 1, kErrorType));
    if (error == nullptr) {
        return 0;
    }
    error->~Error();
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

int Runtime::runBody(lua_State* L) {
    const auto& body = *static_cast<std::function<void(lua_State*)>*>(lua_touserdata(L, lua_upvalueindex(1)));
    // clang-format off
    return Binding::guarded(L, [&] {
        body(L);
        return 0;
    });
    // clang-format on
}

void Runtime::protectedRun(lua_State* L, std::function<void(lua_State*)> body) {
    lua_pushlightuserdata(L, &body);
    lua_pushcclosure(L, &runBody, 1);
    protectedCall(L, 0, 0);
}

} // namespace haylen::lua
