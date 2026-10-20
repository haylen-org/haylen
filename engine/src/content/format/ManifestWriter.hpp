#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "content/crypto/ContentKey.hpp"
#include "content/crypto/SigningKey.hpp"
#include "content/format/Manifest.hpp"

namespace haylen::content {

// Writes the signed manifests of releases on the machines that build and publish them.
class ManifestWriter final {
  public:
    // Encrypts a plain catalog under the content key, signs the envelope with the signing key, and returns the bytes of the manifest. The key IDs of the envelope come from the keys.
    [[nodiscard]] static std::vector<std::uint8_t> write(Manifest::Envelope fields, std::span<const std::uint8_t> catalog, const ContentKey& contentKey, const SigningKey& signingKey);

  private:
    // The envelope without its catalog fields, which is the additional data of the catalog encryption.
    [[nodiscard]] static std::vector<std::uint8_t> writeContext(const Manifest::Envelope& fields, std::uint32_t envelopeSize);
};

} // namespace haylen::content
