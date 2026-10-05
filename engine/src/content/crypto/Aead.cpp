#include "content/crypto/Aead.hpp"

#include <monocypher.h>

namespace haylen::content {

Aead::Tag Aead::seal(Key key, const Nonce& nonce, std::span<const std::uint8_t> additional, std::span<const std::uint8_t> plain, std::span<std::uint8_t> cipher) noexcept {
    Tag tag{};
    crypto_aead_lock(cipher.data(), tag.data(), key.data(), nonce.data(), additional.data(), additional.size(), plain.data(), plain.size());
    return tag;
}

bool Aead::open(Key key, const Nonce& nonce, std::span<const std::uint8_t> additional, const Tag& tag, std::span<const std::uint8_t> cipher, std::span<std::uint8_t> plain) noexcept {
    return crypto_aead_unlock(plain.data(), tag.data(), key.data(), nonce.data(), additional.data(), additional.size(), cipher.data(), cipher.size()) == 0;
}

} // namespace haylen::content
