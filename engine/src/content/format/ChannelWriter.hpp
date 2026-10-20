#pragma once

#include <cstdint>
#include <vector>

#include "content/crypto/SigningKey.hpp"
#include "content/format/ChannelDescriptor.hpp"

namespace haylen::content {

// Writes the signed pointers of update channels on the machines that publish releases.
class ChannelWriter final {
  public:
    // Signs the descriptor with the signing key, whose ID it records, and returns its bytes.
    [[nodiscard]] static std::vector<std::uint8_t> write(const ChannelDescriptor& descriptor, const SigningKey& signingKey);
};

} // namespace haylen::content
