#include "content/crypto/KeyRing.hpp"

#include "content/Error.hpp"

namespace haylen::content {

const Digest& KeyRing::add(std::span<const std::uint8_t, ContentKey::kSize> master) {
    auto key = std::make_shared<const ContentKey>(master);
    const Digest id = key->getId();
    return keys.try_emplace(id, std::move(key)).first->first;
}

bool KeyRing::contains(const Digest& id) const noexcept {
    return keys.contains(id);
}

std::shared_ptr<const ContentKey> KeyRing::get(const Digest& id) const {
    const auto found = keys.find(id);
    if (found == keys.end()) {
        throw Error(Error::Code::UnknownKeyId, "The content key \"" + id.toHex() + "\" is not available to this app. Install the app build that matches its content.");
    }
    return found->second;
}

} // namespace haylen::content
