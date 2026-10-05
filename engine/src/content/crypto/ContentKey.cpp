#include "content/crypto/ContentKey.hpp"

#include <monocypher.h>

#include "content/crypto/Hasher.hpp"

namespace haylen::content {

ContentKey::ContentKey(std::span<const std::uint8_t, kSize> master) noexcept {
    id = Hasher(Digest::kSize, master).updateLabel(kIdLabel).finish();
    for (std::size_t purpose = 0; purpose < kPurposeLabels.size(); ++purpose) {
        Hasher(kSize, master).updateLabel(kPurposeLabels[purpose]).update(id).finish(subkeys[purpose]);
    }
}

ContentKey::~ContentKey() {
    crypto_wipe(subkeys.data(), sizeof(subkeys));
}

std::string_view ContentKey::getLabel(Purpose purpose) noexcept {
    return kPurposeLabels[static_cast<std::size_t>(purpose)];
}

Aead::Key ContentKey::getSubkey(Purpose purpose) const noexcept {
    return subkeys[static_cast<std::size_t>(purpose)];
}

Aead::Nonce ContentKey::deriveNonce(Purpose purpose, std::span<const std::uint8_t> identity) const noexcept {
    Aead::Nonce nonce{};
    Hasher(Aead::kNonceSize, getSubkey(Purpose::Nonce)).updateLabel(getLabel(purpose)).update(identity).finish(nonce);
    return nonce;
}

} // namespace haylen::content
