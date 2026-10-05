#include "content/format/ChunkRecord.hpp"

#include <monocypher.h>

#include <algorithm>
#include <utility>

#include "content/Error.hpp"
#include "content/crypto/Hasher.hpp"
#include "content/format/BinaryReader.hpp"
#include "content/format/BinaryWriter.hpp"

namespace haylen::content {

Digest ChunkRecord::identifyContent(std::span<const std::uint8_t> plain) noexcept {
    return Digest::of(plain);
}

Digest ChunkRecord::identifyStored(Compression::Codec storedCodec, std::uint8_t storedProfile, std::span<const std::uint8_t> encoded) noexcept {
    const std::array<std::uint8_t, 2> method = {static_cast<std::uint8_t>(storedCodec), storedProfile};
    return Hasher().update(method).update(encoded).finish();
}

bool ChunkRecord::hasValidSizes(Compression::Codec storedCodec, std::uint64_t plain, std::uint64_t encoded) noexcept {
    if (plain == 0 || plain > Chunker::kMaximumSize) {
        return false;
    }
    return storedCodec == Compression::Codec::None ? encoded == plain : encoded > 0 && encoded < plain;
}

ChunkRecord ChunkRecord::seal(const ContentKey& key, std::span<const std::uint8_t> plain, const Digest& plainId, std::vector<std::uint8_t>& ciphertext) {
    Compression::Encoded encoded = Compression::encode(plain);
    ChunkRecord record;
    record.codec = encoded.codec;
    record.profile = encoded.profile;
    record.plainSize = plain.size();
    record.encodedSize = encoded.bytes.size();
    record.contentId = plainId;
    record.storedId = identifyStored(encoded.codec, encoded.profile, encoded.bytes);

    // The nonce covers every field before it, and the encryption authenticates every field before the tag.
    const Header identity = record.serialize();
    record.nonce = key.deriveNonce(ContentKey::Purpose::Chunk, std::span(identity).first(kNonceOffset));
    const Header additional = record.serialize();
    ciphertext = std::move(encoded.bytes);
    record.tag = Aead::seal(key.getSubkey(ContentKey::Purpose::Chunk), record.nonce, std::span(additional).first(kTagOffset), ciphertext, ciphertext);
    return record;
}

ChunkRecord ChunkRecord::parse(std::span<const std::uint8_t, kHeaderSize> header) {
    BinaryReader reader(header, Error::Code::CorruptChunk, "chunk record");
    if (!std::ranges::equal(reader.readBytes(kMagic.size()), kMagic)) {
        throw Error(Error::Code::UnsupportedFormat, "The chunk record does not start with the HPAK record magic.");
    }
    if (reader.read<std::uint16_t>() != kVersion) {
        throw Error(Error::Code::UnsupportedVersion, "The chunk record has a version this app does not read.");
    }

    const auto codecByte = reader.read<std::uint8_t>();
    const auto profileByte = reader.read<std::uint8_t>();
    if (!Compression::isSupported(codecByte, profileByte)) {
        reader.fail("names a codec this app does not decode");
    }

    ChunkRecord record;
    record.codec = static_cast<Compression::Codec>(codecByte);
    record.profile = profileByte;
    record.plainSize = reader.read<std::uint64_t>();
    record.encodedSize = reader.read<std::uint64_t>();
    if (!hasValidSizes(record.codec, record.plainSize, record.encodedSize)) {
        reader.fail("declares sizes no chunk has");
    }
    record.contentId = reader.readDigest();
    record.storedId = reader.readDigest();
    std::ranges::copy(reader.readBytes(Aead::kNonceSize), record.nonce.begin());
    std::ranges::copy(reader.readBytes(Aead::kTagSize), record.tag.begin());
    return record;
}

ChunkRecord::Header ChunkRecord::serialize() const {
    BinaryWriter writer;
    writer.writeBytes(kMagic).write(kVersion).write(static_cast<std::uint8_t>(codec)).write(profile);
    writer.write(plainSize).write(encodedSize).writeDigest(contentId).writeDigest(storedId).writeBytes(nonce).writeBytes(tag);

    Header header{};
    std::ranges::copy(writer.getBytes(), header.begin());
    return header;
}

void ChunkRecord::open(const ContentKey& key, std::span<std::uint8_t> ciphertext, std::vector<std::uint8_t>& plain) const {
    const Header additional = serialize();
    if (ciphertext.size() != encodedSize || !Aead::open(key.getSubkey(ContentKey::Purpose::Chunk), nonce, std::span(additional).first(kTagOffset), tag, ciphertext, ciphertext)) {
        throw Error(Error::Code::ChunkAuthenticationFailed, "The chunk \"" + storedId.toHex() + "\" failed authentication, so its shard is damaged or was changed.");
    }

    // The decrypted encoded bytes are wiped as soon as they are decoded, and output that fails its check never leaves.
    plain.resize(static_cast<std::size_t>(plainSize));
    try {
        Compression::decode(codec, ciphertext, plain);
    } catch (...) {
        crypto_wipe(ciphertext.data(), ciphertext.size());
        crypto_wipe(plain.data(), plain.size());
        plain.clear();
        throw;
    }
    crypto_wipe(ciphertext.data(), ciphertext.size());

    if (identifyContent(plain) != contentId) {
        crypto_wipe(plain.data(), plain.size());
        plain.clear();
        throw Error(Error::Code::ChunkHashMismatch, "The chunk \"" + storedId.toHex() + "\" decoded to bytes other than its content ID names.");
    }
}

} // namespace haylen::content
