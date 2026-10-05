#include "content/crypto/VerifyingKey.hpp"

#include <monocypher-ed25519.h>

#include <algorithm>

#include "content/crypto/Hasher.hpp"

namespace haylen::content {

VerifyingKey::VerifyingKey(std::span<const std::uint8_t, kSize> source) noexcept {
    std::copy(source.begin(), source.end(), bytes.begin());
    id = Hasher().updateLabel("haylen/hpak/v1/signing-key-id").update(bytes).finish();
}

bool VerifyingKey::verify(std::span<const std::uint8_t> message, const Signature& signature) const noexcept {
    return crypto_ed25519_check(signature.data(), bytes.data(), message.data(), message.size()) == 0;
}

} // namespace haylen::content
