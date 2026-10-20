#pragma once

#include <lua.hpp>

#include <cstddef>
#include <string>
#include <string_view>

namespace haylen::content {

// The format of the binary chunks of the Lua of this build of the engine, which the module loader checks before it loads the bytecode of a protected release.
class LuaBytecodeAbi final {
  public:
    // Names the format from the header that Lua writes into every chunk: the version of Lua, its format, the sizes of its integers, instructions and numbers, and the byte order, such as `lua-5.5-f0-i4-x4-l8-n8-le`. Bytecode loads only into a build with the same ABI.
    [[nodiscard]] static std::string get();

    // The writer of `lua_dump` that appends the bytes of a chunk to the `std::vector<std::uint8_t>` that `output` points at.
    static int append(lua_State* L, const void* bytes, std::size_t size, void* output);

  private:
    static constexpr std::string_view kSignature = LUA_SIGNATURE;
    static constexpr std::size_t kConversionCheckSize = 6;

    [[nodiscard]] static std::string read();
};

} // namespace haylen::content
