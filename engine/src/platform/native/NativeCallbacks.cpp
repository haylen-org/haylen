#include "platform/native/NativeCallbacks.hpp"

#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

#include "haylen/lua/Runtime.hpp"

namespace haylen::platform {

NativeCallbacks::NativeCallbacks(lua_State* L) : state(lua::Runtime::getMainThread(L)) {}

std::uint64_t NativeCallbacks::add(lua::Reference function) {
    const std::uint64_t id = nextId++;
    functions.emplace(id, std::move(function));
    return id;
}

void NativeCallbacks::remove(std::uint64_t id) {
    functions.erase(id);
}

// The function may free its own callback, so it is on the stack before anything can drop its reference.
void NativeCallbacks::deliver(std::uint64_t id, const std::vector<NativeCallback::Value>& values) {
    const auto found = functions.find(id);
    if (found == functions.end()) {
        return;
    }

    lua_State* L = state;
    const int top = lua_gettop(L);
    // clang-format off
    lua::Runtime::runReporting(L, [&] {
        if (lua_checkstack(L, static_cast<int>(values.size()) + 2) == 0) {
            throw std::runtime_error("A native callback has more arguments than the Lua stack holds.");
        }
        found->second.push(L);
        for (const NativeCallback::Value& value : values) {
            push(L, value);
        }
        lua::Runtime::protectedCall(L, static_cast<int>(values.size()), 0);
    });
    // clang-format on
    lua_settop(L, top);
}

void NativeCallbacks::fail(std::uint64_t id, const std::string& message) {
    if (functions.contains(id)) {
        lua::Runtime::reportError(state, std::runtime_error(message));
    }
}

void NativeCallbacks::push(lua_State* L, const NativeCallback::Value& value) {
    // clang-format off
    std::visit([L](const auto& content) {
        using Content = std::decay_t<decltype(content)>;
        if constexpr (std::is_same_v<Content, std::monostate>) {
            lua_pushnil(L);
        } else if constexpr (std::is_same_v<Content, bool>) {
            lua_pushboolean(L, content ? 1 : 0);
        } else if constexpr (std::is_same_v<Content, std::int64_t>) {
            lua_pushinteger(L, static_cast<lua_Integer>(content));
        } else if constexpr (std::is_same_v<Content, double>) {
            lua_pushnumber(L, content);
        } else if constexpr (std::is_same_v<Content, void*>) {
            lua_pushlightuserdata(L, content);
        } else {
            lua_pushlstring(L, content.data(), content.size());
        }
    }, value);
    // clang-format on
}

} // namespace haylen::platform
