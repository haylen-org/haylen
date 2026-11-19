#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace haylen::test {

// Reads the sound files of the test data.
class AudioFiles final {
  public:
    [[nodiscard]] static std::vector<std::uint8_t> read(std::string_view name);
};

} // namespace haylen::test
