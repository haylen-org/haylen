#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace haylen::content {

// XChaCha20-Poly1305 authenticated encryption: a 256-bit key, a 192-bit nonce and a 128-bit tag that authenticates the ciphertext with its additional data. The ciphertext has the size of the plaintext, and encryption and decryption may work in place.
class Aead final {
  public:
    static constexpr std::size_t kKeySize = 32;
    static constexpr std::size_t kNonceSize = 24;
    static constexpr std::size_t kTagSize = 16;

    using Key = std::span<const std::uint8_t, kKeySize>;
    using Nonce = std::array<std::uint8_t, kNonceSize>;
    using Tag = std::array<std::uint8_t, kTagSize>;

    [[nodiscard]] static Tag seal(Key key, const Nonce& nonce, std::span<const std::uint8_t> additional, std::span<const std::uint8_t> plain, std::span<std::uint8_t> cipher) noexcept;

    // Decrypts only when the tag authenticates the ciphertext and the additional data, and leaves the output untouched otherwise.
    [[nodiscard]] static bool open(Key key, const Nonce& nonce, std::span<const std::uint8_t> additional, const Tag& tag, std::span<const std::uint8_t> cipher, std::span<std::uint8_t> plain) noexcept;
};

} // namespace haylen::content
