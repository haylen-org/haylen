#include "content/crypto/Digest.hpp"

#include <monocypher.h>

#include <algorithm>
#include <string_view>

namespace haylen::content {

Digest::Digest(std::span<const std::uint8_t, kSize> source) noexcept {
    std::copy(source.begin(), source.end(), bytes.begin());
}

Digest Digest::of(std::span<const std::uint8_t> message) noexcept {
    Digest digest;
    crypto_blake2b(digest.bytes.data(), kSize, message.data(), message.size());
    return digest;
}

bool Digest::isZero() const noexcept {
    return std::ranges::all_of(bytes, [](std::uint8_t byte) { return byte == 0; });
}

std::string Digest::toHex() const {
    static constexpr std::string_view kDigits = "0123456789abcdef";
    std::string text;
    text.reserve(kSize * 2);
    for (const std::uint8_t byte : bytes) {
        text.push_back(kDigits[byte >> 4]);
        text.push_back(kDigits[byte & 0x0F]);
    }
    return text;
}

std::size_t Digest::Hash::operator()(const Digest& digest) const noexcept {
    // The bytes of a digest are already uniformly distributed, so its first eight make a good hash.
    std::size_t value = 0;
    for (std::size_t index = 0; index < sizeof(std::size_t); ++index) {
        value = (value << 8) | digest.bytes[index];
    }
    return value;
}

} // namespace haylen::content
