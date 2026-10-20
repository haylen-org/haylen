#include "content/format/RecordSealer.hpp"

#include <utility>

#include "content/crypto/Aead.hpp"
#include "content/format/Compressor.hpp"

namespace haylen::content {

ChunkRecord RecordSealer::seal(const ContentKey& key, std::span<const std::uint8_t> plain, const Digest& plainId, std::vector<std::uint8_t>& ciphertext) {
    Compressor::Encoded encoded = Compressor::encode(plain);
    ChunkRecord record;
    record.codec = encoded.codec;
    record.profile = encoded.profile;
    record.plainSize = plain.size();
    record.encodedSize = encoded.bytes.size();
    record.contentId = plainId;
    record.storedId = ChunkRecord::identifyStored(encoded.codec, encoded.profile, encoded.bytes);

    // The nonce covers every field before it, and the encryption authenticates every field before the tag.
    const ChunkRecord::Header identity = record.serialize();
    record.nonce = key.deriveNonce(ContentKey::Purpose::Chunk, std::span(identity).first(ChunkRecord::kNonceOffset));
    const ChunkRecord::Header additional = record.serialize();
    ciphertext = std::move(encoded.bytes);
    record.tag = Aead::seal(key.getSubkey(ContentKey::Purpose::Chunk), record.nonce, std::span(additional).first(ChunkRecord::kTagOffset), ciphertext, ciphertext);
    return record;
}

} // namespace haylen::content
