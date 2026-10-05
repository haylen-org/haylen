#pragma once

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace haylen::content {

// A 256-bit BLAKE2b digest. Content IDs, stored IDs, key IDs, shard IDs and manifest IDs are all digests, and an all-zero digest means none.
class Digest final {
  public:
    static constexpr std::size_t kSize = 32;

    struct Hash {
        [[nodiscard]] std::size_t operator()(const Digest& digest) const noexcept;
    };

    Digest() = default;
    explicit Digest(std::span<const std::uint8_t, kSize> source) noexcept;

    // Hashes bytes with BLAKE2b-256 and no key.
    [[nodiscard]] static Digest of(std::span<const std::uint8_t> message) noexcept;

    [[nodiscard]] std::span<const std::uint8_t, kSize> getBytes() const noexcept {
        return bytes;
    }
    [[nodiscard]] bool isZero() const noexcept;
    [[nodiscard]] std::string toHex() const;

    friend bool operator==(const Digest&, const Digest&) = default;
    friend std::strong_ordering operator<=>(const Digest&, const Digest&) = default;

  private:
    std::array<std::uint8_t, kSize> bytes{};
};

} // namespace haylen::content
