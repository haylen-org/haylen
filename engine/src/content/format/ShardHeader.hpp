#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "content/crypto/Aead.hpp"
#include "content/crypto/ContentKey.hpp"
#include "content/crypto/Digest.hpp"

namespace haylen::content {

// The fixed header of 160 bytes at the start of an HPAK shard. It names the shard and its content key and locates the encrypted index at the end of the file, and it is the additional data of that index, so the index authenticates the header too.
class ShardHeader final {
  public:
    static constexpr std::size_t kSize = 160;
    static constexpr std::uint16_t kVersion = 1;

    using Bytes = std::array<std::uint8_t, kSize>;

    Digest shardId;
    Digest keyId;
    std::uint64_t indexOffset = 0;
    std::uint64_t indexSize = 0;
    std::uint64_t entryCount = 0;
    Aead::Nonce indexNonce{};
    Aead::Tag indexTag{};

    // Reads the header of a shard file of the given size and checks that the index fills the end of the file. Throws `UnsupportedFormat`, `UnsupportedVersion` or `CorruptHeader`.
    [[nodiscard]] static ShardHeader parse(std::span<const std::uint8_t, kSize> bytes, std::uint64_t fileSize);

    [[nodiscard]] Bytes serialize() const;

    // Derives the nonce of the plain index, encrypts it in place and records the nonce and the tag in the header.
    void sealIndex(const ContentKey& key, std::span<std::uint8_t> index);

    // Authenticates and decrypts the index in place, and throws `CorruptIndex` when it fails.
    void openIndex(const ContentKey& key, std::span<std::uint8_t> index) const;

  private:
    static constexpr std::array<std::uint8_t, 8> kMagic = {'H', 'P', 'A', 'K', 0x0D, 0x0A, 0x1A, 0x0A};
    static constexpr std::size_t kNonceOffset = 104;
    static constexpr std::size_t kTagOffset = 144;
    static constexpr std::size_t kReservedSize = 16;
};

} // namespace haylen::content
