#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

#include "haylen/content/KeyProvider.hpp"

namespace haylen::content {

// The key provider that the bootstrap of a release build compiles into an app: every content key of the app as two binary constants, a mask and the key sealed with a digest of the mask, so the binary never holds a key, nor its hexadecimal or Base64 text, and a key exists in memory only while the engine derives its subkeys from it. It keeps the keys away from tools that search binaries for them, and it is no cryptographic protection against someone who studies the code of the app, which must hold its keys to run offline.
class EmbeddedKeyProvider final : public KeyProvider {
  public:
    using Mask = std::array<std::uint8_t, kSize>;

    struct SealedKey {
        KeyId id{};
        Mask mask{};
        Key sealed{};
    };

    explicit EmbeddedKeyProvider(std::vector<SealedKey> sealedKeys);

    // Seals a key with a mask that derives from the key itself, so the same key always seals to the same constants. The content tool calls it when it writes the bootstrap of an app.
    [[nodiscard]] static SealedKey seal(const KeyId& id, const Key& key);

    [[nodiscard]] std::vector<KeyId> getKeyIds() const override;
    [[nodiscard]] bool getKey(const KeyId& id, Key& target) const override;

  private:
    static constexpr std::string_view kMaskLabel = "haylen/bootstrap/v1/mask";
    static constexpr std::string_view kPadLabel = "haylen/bootstrap/v1/pad";

    // Combines a sealed or plain key with the pad that a digest of its mask and ID makes, which turns one into the other.
    static void applyPad(const SealedKey& key, const Key& source, Key& target) noexcept;

    std::vector<SealedKey> keys;
};

} // namespace haylen::content
