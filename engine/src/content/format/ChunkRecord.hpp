#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "content/crypto/Aead.hpp"
#include "content/crypto/ContentKey.hpp"
#include "content/crypto/Digest.hpp"
#include "content/format/Chunker.hpp"
#include "content/format/Compression.hpp"

namespace haylen::content {

// One stored chunk of an HPAK shard: a fixed header of 128 bytes followed by the ciphertext of the encoded chunk. The header is the additional data of the encryption, so none of its fields changes without failing authentication, and the nonce derives from the header, so the same chunk always seals to the same bytes.
class ChunkRecord final {
  public:
    static constexpr std::size_t kHeaderSize = 128;
    static constexpr std::uint16_t kVersion = 1;

    using Header = std::array<std::uint8_t, kHeaderSize>;

    Compression::Codec codec = Compression::Codec::None;
    std::uint8_t profile = Compression::kNoneProfile;
    std::uint64_t plainSize = 0;
    std::uint64_t encodedSize = 0;
    Digest contentId;
    Digest storedId;
    Aead::Nonce nonce{};
    Aead::Tag tag{};

    // The ID of a chunk by its plain bytes.
    [[nodiscard]] static Digest identifyContent(std::span<const std::uint8_t> plain) noexcept;

    // The ID of a chunk by the way it is stored: its codec, its profile and its encoded bytes.
    [[nodiscard]] static Digest identifyStored(Compression::Codec storedCodec, std::uint8_t storedProfile, std::span<const std::uint8_t> encoded) noexcept;

    // Encodes and encrypts a plain chunk whose content ID the caller computed already, and replaces the ciphertext with the bytes that follow the header.
    [[nodiscard]] static ChunkRecord seal(const ContentKey& key, std::span<const std::uint8_t> plain, const Digest& plainId, std::vector<std::uint8_t>& ciphertext);

    // Tells whether sizes suit a chunk: a plain size from one byte to the maximum chunk size, the same encoded size when stored as it is, and a smaller one when compressed.
    [[nodiscard]] static bool hasValidSizes(Compression::Codec storedCodec, std::uint64_t plain, std::uint64_t encoded) noexcept;

    // Reads a header, and throws `UnsupportedFormat`, `UnsupportedVersion` or `CorruptChunk` when it is not a valid version 1 header.
    [[nodiscard]] static ChunkRecord parse(std::span<const std::uint8_t, kHeaderSize> header);

    [[nodiscard]] Header serialize() const;

    // Authenticates and decrypts the ciphertext in place, then decodes it into the plain chunk, resized to the plain size, and checks its content ID. Throws `ChunkAuthenticationFailed` or `ChunkHashMismatch`, and never leaves unverified bytes in the output.
    void open(const ContentKey& key, std::span<std::uint8_t> ciphertext, std::vector<std::uint8_t>& plain) const;

    [[nodiscard]] std::uint64_t getRecordSize() const noexcept {
        return kHeaderSize + encodedSize;
    }

  private:
    static constexpr std::array<std::uint8_t, 4> kMagic = {'H', 'P', 'K', 'R'};
    static constexpr std::size_t kNonceOffset = 88;
    static constexpr std::size_t kTagOffset = 112;
};

} // namespace haylen::content
