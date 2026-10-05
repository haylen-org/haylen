#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "content/crypto/Digest.hpp"

namespace haylen::content {

// Writes the fields of the content formats: unsigned integers of fixed width in little-endian order, raw bytes and strings with a 16-bit length.
class BinaryWriter final {
  public:
    template <std::unsigned_integral T> BinaryWriter& write(T value) {
        for (std::size_t index = 0; index < sizeof(T); ++index) {
            bytes.push_back(static_cast<std::uint8_t>(value >> (8 * index)));
        }
        return *this;
    }

    BinaryWriter& writeBytes(std::span<const std::uint8_t> data);
    BinaryWriter& writeDigest(const Digest& digest);
    BinaryWriter& writeString(std::string_view text);
    BinaryWriter& writeZeros(std::size_t count);

    [[nodiscard]] std::size_t size() const noexcept {
        return bytes.size();
    }
    [[nodiscard]] const std::vector<std::uint8_t>& getBytes() const noexcept {
        return bytes;
    }
    [[nodiscard]] std::vector<std::uint8_t> take() noexcept {
        return std::move(bytes);
    }

  private:
    std::vector<std::uint8_t> bytes;
};

} // namespace haylen::content
