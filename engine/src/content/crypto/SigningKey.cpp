#include "content/crypto/SigningKey.hpp"

#include <monocypher-ed25519.h>

#include <algorithm>

namespace haylen::content {

SigningKey::SigningKey(std::span<const std::uint8_t, kSeedSize> seed) noexcept : verifyingKey(derive(seed, secretKey)) {}

SigningKey::~SigningKey() {
    crypto_wipe(secretKey.data(), secretKey.size());
}

VerifyingKey SigningKey::derive(std::span<const std::uint8_t, kSeedSize> seed, std::array<std::uint8_t, kSecretSize>& secret) noexcept {
    // Monocypher wipes the seed it derives from, so it works on a copy.
    std::array<std::uint8_t, kSeedSize> copy{};
    std::copy(seed.begin(), seed.end(), copy.begin());
    std::array<std::uint8_t, VerifyingKey::kSize> publicKey{};
    crypto_ed25519_key_pair(secret.data(), publicKey.data(), copy.data());
    return VerifyingKey(publicKey);
}

VerifyingKey::Signature SigningKey::sign(std::span<const std::uint8_t> message) const noexcept {
    VerifyingKey::Signature signature{};
    crypto_ed25519_sign(signature.data(), secretKey.data(), message.data(), message.size());
    return signature;
}

} // namespace haylen::content
