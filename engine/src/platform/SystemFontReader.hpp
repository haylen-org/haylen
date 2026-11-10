#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace haylen::platform {

// Reads the font files of the system for the platforms that name the file of a face, Windows, Linux and Android.
class SystemFontReader final {
  public:
    // Reads the whole file, which throws `std::runtime_error` when the system does not let the app read it.
    [[nodiscard]] static std::vector<std::uint8_t> read(const std::filesystem::path& file);
};

} // namespace haylen::platform
