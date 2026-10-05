#pragma once

#include <monocypher.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "content/crypto/Digest.hpp"

namespace haylen::content {

// Hashes a message given in parts with BLAKE2b, with or without a key. Keyed hashes derive keys and nonces, and the state is wiped when the hasher ends.
class Hasher final {
  public:
    explicit Hasher(std::size_t outputSize = Digest::kSize, std::span<const std::uint8_t> key = {}) noexcept;
    ~Hasher();

    Hasher(const Hasher&) = delete;
    Hasher& operator=(const Hasher&) = delete;

    Hasher& update(std::span<const std::uint8_t> bytes) noexcept;

    // Adds a label as its length in one byte followed by its characters, so labels never run into the parts after them.
    Hasher& updateLabel(std::string_view label) noexcept;
    Hasher& update(const Digest& digest) noexcept;

    // Writes the hash, whose size is the output size of the hasher.
    void finish(std::span<std::uint8_t> output) noexcept;
    [[nodiscard]] Digest finish() noexcept;

  private:
    crypto_blake2b_ctx context;
};

} // namespace haylen::content
