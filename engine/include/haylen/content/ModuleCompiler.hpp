#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace haylen::content {

// Compiles the modules of the scripts of an app for the app domain of a release, so a release ships no text of its scripts. The format of releases never depends on the language of the scripts, so the Lua layer brings the compiler of Lua modules, which the content tool hands the release builder.
class ModuleCompiler {
  public:
    virtual ~ModuleCompiler() = default;

    // Names the format of the compiled modules, which the manifest of the app domain records and the bootstrap of the build that loads them names too, such as `lua-5.5-f0-i4-x4-l8-n8-le`.
    [[nodiscard]] virtual std::string getAbi() const = 0;

    // Compiles the text of a module, named by its package path. Throws `std::invalid_argument` with the reason, which names the module and the line, for a module that does not compile.
    [[nodiscard]] virtual std::vector<std::uint8_t> compile(std::string_view source, std::string_view path) const = 0;
};

} // namespace haylen::content
