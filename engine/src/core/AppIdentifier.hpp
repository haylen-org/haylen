#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace haylen::core {

// The identifier of an app whose `haylen.json` names none, by the rule that the tooling follows for an app folder: `local.`, the letters and digits of the last part of the location of the package and eight hexadecimal digits of the BLAKE2b hash of the whole location, such as `local.skygarden3fa9c1d2`. The location is the resolved path of a folder or a zip file on disk, the address that a launched package came from, or the location that the creator of a package in memory names, so two packages never share an identifier or their storage however they run.
class AppIdentifier final {
  public:
    static constexpr std::string_view kPrefix = "local.";

    // The letters of a package whose name holds none.
    static constexpr std::string_view kFallback = "haylenapp";
    static constexpr std::size_t kMaxName = 40;
    static constexpr std::size_t kDigestSize = 4;

    // Composes the identifier of a location given as UTF-8 text. A location that holds `://` is an address, whose name is its last part without its query and fragment.
    [[nodiscard]] static std::string fromLocation(std::string_view location);

  private:
    [[nodiscard]] static std::string_view nameOf(std::string_view location);
};

} // namespace haylen::core
