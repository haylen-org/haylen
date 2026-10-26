#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace haylen::core {

// Reads the Base64 text of data files, such as the tile layers of maps and the SPIR-V modules of shader packages.
class Base64 final {
  public:
    // Returns the bytes of the text, skipping padding and white space, or nothing when it holds a character outside the Base64 alphabet.
    [[nodiscard]] static std::optional<std::vector<std::uint8_t>> decode(std::string_view text);

  private:
    [[nodiscard]] static std::array<int, 256> makeTable() noexcept;
};

} // namespace haylen::core
