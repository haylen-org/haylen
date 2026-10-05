#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "content/crypto/Aead.hpp"
#include "content/crypto/ContentKey.hpp"
#include "content/crypto/Digest.hpp"
#include "content/crypto/KeyRing.hpp"
#include "content/crypto/SigningKey.hpp"
#include "content/crypto/VerifyingKey.hpp"
#include "content/format/Catalog.hpp"
#include "content/format/ShardReference.hpp"

namespace haylen::content {

// A signed `.hmanifest`: a public envelope that names the app, the domain, the generation, the compatibility range, the keys and the shards, its Ed25519 signature, and the encrypted catalog. The envelope never names a path. Reading verifies the signature before it trusts any field but the size of the envelope.
class Manifest final {
  public:
    static constexpr std::uint16_t kVersion = 1;
    static constexpr std::size_t kMaximumNameLength = 64;
    static constexpr std::uint64_t kMaximumShards = std::uint64_t{1} << 20;
    static constexpr std::uint64_t kMaximumCatalogSize = std::uint64_t{512} << 20;

    // The app domain holds `app.json`, the Lua modules and the plugins, which belong to one build of the app. The content domain holds the assets, which updates may replace.
    enum class Domain : std::uint8_t {
        App = 0,
        Content = 1,
    };

    struct Envelope {
        Domain domain = Domain::Content;
        Digest application;
        std::string profile;
        std::string channel;

        // The ABI of the Lua bytecode that the catalog holds, empty when it holds none.
        std::string luaAbi;
        std::uint64_t generation = 0;
        Digest previousManifest;
        std::uint64_t minimumAppBuild = 0;
        std::uint64_t maximumAppBuild = 0;
        Digest contentKeyId;
        Digest signingKeyId;
        std::vector<ShardReference> shards;
    };

    // The digest that names an app in manifests, from its identifier.
    [[nodiscard]] static Digest identifyApplication(std::string_view identifier) noexcept;

    // Tells whether a package path names a Lua module of the app or of a plugin, the only files that may be Lua bytecode.
    [[nodiscard]] static bool isLuaModule(std::string_view path) noexcept;

    // Encrypts a plain catalog under the content key, signs the envelope with the signing key, and returns the bytes of the manifest. The key IDs of the envelope come from the keys.
    [[nodiscard]] static std::vector<std::uint8_t> write(Envelope fields, std::span<const std::uint8_t> catalog, const ContentKey& contentKey, const SigningKey& signingKey);

    // Verifies the signature with the trusted key of the signing key ID and reads the envelope. Throws `UnsupportedFormat`, `UnsupportedVersion`, `CorruptManifest` or `ManifestSignatureInvalid`.
    [[nodiscard]] static Manifest read(std::vector<std::uint8_t> file, std::span<const VerifyingKey> trustedKeys);

    [[nodiscard]] const Envelope& getEnvelope() const noexcept {
        return envelope;
    }

    // The ID of a manifest is the digest of its signed envelope.
    [[nodiscard]] const Digest& getId() const noexcept {
        return id;
    }

    // Checks the catalog against the digest the envelope signs, decrypts it with the content key the envelope names, and checks that every file belongs to the domain of the manifest and that only Lua modules of the app domain are Lua bytecode, of the ABI the envelope names. Throws `UnknownKeyId`, `CatalogAuthenticationFailed` or `CorruptCatalog`.
    [[nodiscard]] Catalog decryptCatalog(const KeyRing& keys) const;

  private:
    static constexpr std::array<std::uint8_t, 8> kMagic = {'H', 'M', 'A', 'N', 0x0D, 0x0A, 0x1A, 0x0A};
    static constexpr std::size_t kPrefixSize = 16;
    static constexpr std::size_t kSigningKeyOffset = 56;
    static constexpr std::size_t kShardReferenceSize = 2 * Digest::kSize + 8;
    static constexpr std::size_t kCatalogFieldsSize = 8 + Digest::kSize + Aead::kNonceSize + Aead::kTagSize;
    static constexpr std::uint32_t kMaximumEnvelopeSize = 128U << 20;

    Manifest() = default;

    // The envelope without its catalog fields, which is the additional data of the catalog encryption.
    [[nodiscard]] static std::vector<std::uint8_t> writeContext(const Envelope& fields, std::uint32_t envelopeSize);
    [[nodiscard]] static bool belongsToDomain(std::string_view path, Domain domain) noexcept;
    void checkFiles(const Catalog& catalog) const;
    [[nodiscard]] static bool isValidName(std::string_view name) noexcept;
    void parseEnvelope(std::span<const std::uint8_t> signedBytes);

    Envelope envelope;
    Digest id;
    std::vector<std::uint8_t> bytes;
    std::size_t contextSize = 0;
    std::uint64_t catalogSize = 0;
    Digest catalogDigest;
    Aead::Nonce catalogNonce{};
    Aead::Tag catalogTag{};
};

} // namespace haylen::content
