#include "haylen/lua/Runtime.hpp"

#include <charconv>
#include <new>
#include <system_error>
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
            frames.push_back({.source = "...", .function = std::to_string(skipped) + " levels skipped", .kind = Error::Frame::Kind::C});
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

        frames.push_back({.source = info.short_src, .line = info.currentline > 0 ? info.currentline : 0, .function = describeFunction(info), .kind = getFrameKind(info)});
    }
    return Error(text, std::move(frames));
}

// The outermost frames that Lua cannot name are the native entries of Varn that ran the task or the callback, which the stack leaves out like the protected calls of the engine.
std::vector<Error::Frame> Runtime::readTraceback(std::string_view traceback) {
    std::vector<Error::Frame> frames;
    for (std::size_t start = traceback.find('\n'); start != std::string_view::npos;) {
        const std::size_t end = traceback.find('\n', start + 1);
        if (std::optional<Error::Frame> frame = readTracebackLine(traceback.substr(start + 1, end == std::string_view::npos ? end : end - start - 1))) {
            frames.push_back(std::move(*frame));
        }
        start = end;
    }

    while (!frames.empty() && frames.back().kind == Error::Frame::Kind::C && frames.back().function == "?") {
        frames.pop_back();
    }
    return frames;
}

// Reads a frame, written as `source:line: in function` or as `source: in function` without a current line, or the marker of skipped levels, and nothing from the header or the note after a tail call.
std::optional<Error::Frame> Runtime::readTracebackLine(std::string_view line) {
    constexpr std::string_view skipped = "\t...\t(skipping ";
    if (line.starts_with(skipped)) {
        const std::string_view count = line.substr(skipped.size(), line.find(' ', skipped.size()) - skipped.size());
        return Error::Frame{.source = "...", .function = std::string(count) + " levels skipped", .kind = Error::Frame::Kind::C};
    }

    constexpr std::string_view separator = ": in ";
    const std::size_t split = line.find(separator);
    if (!line.starts_with('\t') || split == std::string_view::npos) {
        return std::nullopt;
    }

    std::string_view location = line.substr(1, split - 1);
    const std::string_view function = line.substr(split + separator.size());
    int number = 0;
    if (const std::size_t colon = location.rfind(':'); colon != std::string_view::npos) {
        const std::string_view digits = location.substr(colon + 1);
        const auto [last, failure] = std::from_chars(digits.data(), digits.data() + digits.size(), number);
        if (failure == std::errc() && last == digits.data() + digits.size() && number > 0) {
            location = location.substr(0, colon);
        } else {
            number = 0;
        }
    }

    const Error::Frame::Kind kind = location == "[C]" ? Error::Frame::Kind::C : (function == "main chunk" ? Error::Frame::Kind::Main : Error::Frame::Kind::Lua);
    return Error::Frame{.source = std::string(location), .line = number, .function = std::string(function), .kind = kind};
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
