#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "haylen/core/FloatBuffer.hpp"
#include "haylen/lua/Type.hpp"

struct lua_State;

namespace haylen::lua {

template <> struct Type<haylen::core::FloatBuffer> {
    static constexpr const char* name = "haylen.FloatBuffer";
    using Storage = std::shared_ptr<haylen::core::FloatBuffer>;
};

} // namespace haylen::lua

namespace haylen::lua::core {

// Installs the `FloatBuffer` class of `haylen.collections`, whose values count from one like a Lua array. Index access and `#buffer` take the fast path of the binding toolkit, and `set`, `get` and `fill` move many values in one call.
class FloatBufferBinding final {
  public:
    static void install(lua_State* L);

    // Creates a buffer with `collections.newFloatBuffer(size[, value])`.
    static int create(lua_State* L);

  private:
    [[nodiscard]] static haylen::core::FloatBuffer& check(lua_State* L);

    // Reads the one-based position at the stack index and returns it counted from zero, raising an error when `count` values from it do not fit in the buffer.
    [[nodiscard]] static std::size_t checkPosition(lua_State* L, const haylen::core::FloatBuffer& buffer, int index, std::uint64_t count);

    static int at(lua_State* L);
    static int assign(lua_State* L);
    static int length(lua_State* L);
    static int set(lua_State* L);
    static int get(lua_State* L);
    static int fill(lua_State* L);
};

} // namespace haylen::lua::core
