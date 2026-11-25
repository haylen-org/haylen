#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>

namespace haylen::core {

// The identifier of an app whose `haylen.json` names none, by the rule that the tooling follows for an app folder: `local.`, the letters and digits of the name of the folder or the zip file of the package and eight hexadecimal digits of the BLAKE2b hash of its resolved path, such as `local.skygarden3fa9c1d2`. Two packages on one machine never share it, so they never share their storage however they run.
class AppIdentifier final {
  public:
    static constexpr std::string_view kPrefix = "local.";

    // The letters of a package whose name holds none.
    static constexpr std::string_view kFallback = "haylenapp";
    static constexpr std::size_t kMaxName = 40;
    static constexpr std::size_t kDigestSize = 4;

    // Resolves the folder or the zip file of a package on disk and composes its identifier.
    [[nodiscard]] static std::string generate(const std::filesystem::path& location);

    // Composes the identifier of a name and a resolved path, given as UTF-8 text.
    [[nodiscard]] static std::string compose(std::string_view name, std::string_view resolvedPath);
};

} // namespace haylen::core
