#include "haylen/content/EmbeddedKeyProvider.hpp"

#include <monocypher.h>

#include <algorithm>
#include <iterator>
#include <utility>

#include "content/crypto/Hasher.hpp"

namespace haylen::content {

EmbeddedKeyProvider::EmbeddedKeyProvider(std::vector<SealedKey> sealedKeys) : keys(std::move(sealedKeys)) {}

void EmbeddedKeyProvider::applyPad(const SealedKey& key, const Key& source, Key& target) noexcept {
    Key pad{};
    Hasher(kSize).updateLabel(kPadLabel).update(key.mask).update(key.id).finish(pad);
    std::ranges::transform(source, pad, target.begin(), [](std::uint8_t byte, std::uint8_t mask) { return static_cast<std::uint8_t>(byte ^ mask); });
    crypto_wipe(pad.data(), pad.size());
}

EmbeddedKeyProvider::SealedKey EmbeddedKeyProvider::seal(const KeyId& id, const Key& key) {
    SealedKey sealed{.id = id};
    Hasher(kSize, key).updateLabel(kMaskLabel).finish(sealed.mask);
    applyPad(sealed, key, sealed.sealed);
    return sealed;
}

std::vector<KeyProvider::KeyId> EmbeddedKeyProvider::getKeyIds() const {
    std::vector<KeyId> ids;
    std::ranges::transform(keys, std::back_inserter(ids), &SealedKey::id);
    return ids;
}

bool EmbeddedKeyProvider::getKey(const KeyId& id, Key& target) const {
    const auto found = std::ranges::find(keys, id, &SealedKey::id);
    if (found == keys.end()) {
        return false;
    }
    applyPad(*found, found->sealed, target);
    return true;
}

} // namespace haylen::content
