#pragma once

#include <cstdint>
#include <span>

namespace haylen::content {

// Fills buffers from the random generator of the operating system, where the keys of apps come from.
class SecureRandom final {
  public:
    // Throws `std::runtime_error` when the system cannot provide random bytes.
    static void fill(std::span<std::uint8_t> target);

  private:
    static constexpr std::size_t kMaximumRequest = 256;
};

} // namespace haylen::content
