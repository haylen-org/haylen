#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace haylen::content {

// Compiles the Lua modules of an app into the binary chunks of the Lua that the engine runs, for the app domain of a release, so a release ships no Lua text and loads its modules without parsing them. A chunk keeps the package path of its module as its name and all of its debug information, so an error in a release names the module, the line and the local variables as it does in development, and it holds no folder of the machine that built it. Its chunks have the format that `LuaBytecodeAbi` names.
class LuaCompiler final {
  public:
    // Compiles the text of a module, named by its package path, into a binary chunk. Throws `std::invalid_argument` with the message of Lua, which names the module and the line, for a module that does not compile.
    [[nodiscard]] static std::vector<std::uint8_t> compile(std::string_view source, std::string_view path);
};

} // namespace haylen::content
