#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "content/crypto/Digest.hpp"
#include "content/crypto/SigningKey.hpp"
#include "content/crypto/VerifyingKey.hpp"
#include "content/format/Manifest.hpp"

namespace haylen::content {

// The signed pointer of an update channel to its current content manifest, with the generation the publisher raises with every release of the channel. Clients never move to an older generation, so a rollback is published as a new generation that names the earlier content.
class ChannelDescriptor final {
  public:
    static constexpr std::uint16_t kVersion = 1;

    Digest application;
    std::string channel;
    std::uint64_t generation = 0;
    Digest manifest;
    Digest signingKeyId;

    // Signs the descriptor with the signing key, whose ID it records, and returns its bytes.
    [[nodiscard]] std::vector<std::uint8_t> write(const SigningKey& signingKey) const;

    // Verifies the signature with the trusted key of the signing key ID before it reads any other field. Throws `UnsupportedFormat`, `UnsupportedVersion`, `CorruptManifest` or `ManifestSignatureInvalid`.
    [[nodiscard]] static ChannelDescriptor read(std::span<const std::uint8_t> bytes, std::span<const VerifyingKey> trustedKeys);

    // Tells whether the descriptor offers content newer than the generation and manifest the app accepted last. Throws `ManifestRollbackRejected` for an older generation, or for the same generation with another manifest.
    [[nodiscard]] bool offersUpdate(std::uint64_t acceptedGeneration, const Digest& acceptedManifest) const;

    // Tells whether a manifest is the content manifest that the descriptor names for its app, channel and generation.
    [[nodiscard]] bool describes(const Manifest& candidate) const noexcept;

  private:
    static constexpr std::array<std::uint8_t, 8> kMagic = {'H', 'C', 'H', 'N', 0x0D, 0x0A, 0x1A, 0x0A};
    static constexpr std::size_t kSigningKeyOffset = 44;
};

} // namespace haylen::content
