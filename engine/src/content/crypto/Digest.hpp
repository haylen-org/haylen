#pragma once

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

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

    // Hashes a whole file with BLAKE2b-256 in blocks of bounded size, and throws `std::runtime_error` when the file cannot be read.
    [[nodiscard]] static Digest ofFile(const std::filesystem::path& file);

    // Reads the 64 lowercase hexadecimal digits that `toHex` writes, or nothing for any other text.
    [[nodiscard]] static std::optional<Digest> fromHex(std::string_view text) noexcept;

    [[nodiscard]] std::span<const std::uint8_t, kSize> getBytes() const noexcept {
        return bytes;
    }
    [[nodiscard]] bool isZero() const noexcept;
    [[nodiscard]] std::string toHex() const;

    friend bool operator==(const Digest&, const Digest&) = default;
    friend std::strong_ordering operator<=>(const Digest&, const Digest&) = default;

  private:
    static constexpr std::size_t kFileBlockSize = 1024 * 1024;
    static constexpr std::string_view kDigits = "0123456789abcdef";

    std::array<std::uint8_t, kSize> bytes{};
};

} // namespace haylen::content
