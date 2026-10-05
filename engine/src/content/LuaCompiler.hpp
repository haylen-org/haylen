#pragma once

#include <lua.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace haylen::content {

// Compiles the Lua modules of an app into the binary chunks of the Lua that the engine runs, for the app domain of a release, so a release ships no Lua text and loads its modules without parsing them. A chunk keeps the package path of its module as its name and all of its debug information, so an error in a release names the module, the line and the local variables as it does in development, and it holds no folder of the machine that built it.
class LuaCompiler final {
  public:
    // Compiles the text of a module, named by its package path, into a binary chunk. Throws `std::invalid_argument` with the message of Lua, which names the module and the line, for a module that does not compile.
    [[nodiscard]] static std::vector<std::uint8_t> compile(std::string_view source, std::string_view path);

    // Names the format of the binary chunks of this build of the engine, from the header that Lua writes into every chunk: the version of Lua, its format, the sizes of its integers, instructions and numbers, and the byte order, such as `lua-5.5-f0-i4-x4-l8-n8-le`. Bytecode loads only into a build with the same ABI.
    [[nodiscard]] static std::string getAbi();

  private:
    static constexpr std::string_view kSignature = LUA_SIGNATURE;
    static constexpr std::size_t kConversionCheckSize = 6;

    [[nodiscard]] static std::string readAbi();
    static int write(lua_State* L, const void* bytes, std::size_t size, void* output);
};

} // namespace haylen::content
