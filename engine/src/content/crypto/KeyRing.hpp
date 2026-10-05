#pragma once

#include <map>
#include <memory>
#include <span>

#include "content/crypto/ContentKey.hpp"
#include "content/crypto/Digest.hpp"

namespace haylen::content {

// The content keys an app can decrypt with, found by their IDs. Holding more than one key lets content encrypted under a new key coexist with installed content under the previous one while keys rotate.
class KeyRing final {
  public:
    // Adds a key and returns its ID. Adding a key that is already there changes nothing.
    const Digest& add(std::span<const std::uint8_t, ContentKey::kSize> master);

    [[nodiscard]] bool contains(const Digest& id) const noexcept;

    // Throws `UnknownKeyId` naming the ID when no key has it.
    [[nodiscard]] std::shared_ptr<const ContentKey> get(const Digest& id) const;

  private:
    std::map<Digest, std::shared_ptr<const ContentKey>> keys;
};

} // namespace haylen::content
