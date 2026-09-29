#pragma once

#include <lua.hpp>

#include <type_traits>
#include <utility>

#include "haylen/lua/Converter.hpp"

namespace haylen::lua {

// Pushes C++ values onto the Lua stack and reads them back through their converters.
class Stack final {
  public:
    template <typename T> static void push(lua_State* L, T&& value) {
        Converter<std::remove_cvref_t<T>>::push(L, std::forward<T>(value));
    }

    template <typename T> [[nodiscard]] static T read(lua_State* L, int index) {
        return Converter<T>::read(L, index);
    }

    template <typename T> [[nodiscard]] static bool is(lua_State* L, int index) {
        return Converter<T>::is(L, index);
    }
};

} // namespace haylen::lua
