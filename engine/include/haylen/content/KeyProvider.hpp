#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace haylen::content {

// Gives the engine the content keys that decrypt the protected release of an app, by their IDs. The engine asks for every key when it opens the release, derives the subkeys it uses and wipes the key, and no key ever reaches Lua. Release builds compile in an `EmbeddedKeyProvider`, and a C++ app may give one of its own, such as one that fetches keys from the service of an online game.
class KeyProvider {
  public:
    static constexpr std::size_t kSize = 32;

    using KeyId = std::array<std::uint8_t, kSize>;
    using Key = std::array<std::uint8_t, kSize>;

    virtual ~KeyProvider() = default;

    [[nodiscard]] virtual std::vector<KeyId> getKeyIds() const = 0;

    // Writes the key with the ID into the target and returns whether the provider holds it.
    [[nodiscard]] virtual bool getKey(const KeyId& id, Key& target) const = 0;
};

} // namespace haylen::content
