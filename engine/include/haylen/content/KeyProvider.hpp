#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace haylen::content {

// Gives the engine the content keys that decrypt the protected release of an app, by their IDs. The engine takes every key that the provider holds when it opens the release, derives the subkeys it uses and wipes the key, and asks with `requestKey` for a key that content needs later, such as the key of an update. No key ever reaches Lua. Release builds compile in an `EmbeddedKeyProvider`, and a C++ app may give one of its own, such as one that fetches keys from the service of an online game, or a `CompositeKeyProvider` of both.
class KeyProvider {
  public:
    static constexpr std::size_t kSize = 32;

    using KeyId = std::array<std::uint8_t, kSize>;
    using Key = std::array<std::uint8_t, kSize>;

    virtual ~KeyProvider() = default;

    [[nodiscard]] virtual std::vector<KeyId> getKeyIds() const = 0;

    // Writes the key with the ID into the target and returns whether the provider holds it.
    [[nodiscard]] virtual bool getKey(const KeyId& id, Key& target) const = 0;

    // Asks for a key that the provider does not hold yet, such as one that the service of an online game gives once the player signed in, and calls `done` once, from any thread, with whether `getKey` gives it now. A provider that holds its keys from the start answers false at once, as this one does.
    virtual void requestKey(const KeyId& id, std::function<void(bool)> done) const;
};

} // namespace haylen::content
