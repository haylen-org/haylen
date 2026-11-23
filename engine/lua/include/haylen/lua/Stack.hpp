#pragma once

#include <lua.hpp>

#include <type_traits>
#include <utility>
#include <vector>

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

    // Reads a list into storage that the caller keeps, such as storage that every frame reuses.
    template <typename T> static void readInto(lua_State* L, int index, std::vector<T>& values) {
        Converter<std::vector<T>>::readInto(L, index, values);
    }

    template <typename T> [[nodiscard]] static bool is(lua_State* L, int index) {
        return Converter<T>::is(L, index);
    }
};

} // namespace haylen::lua
