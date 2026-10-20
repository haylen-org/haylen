#include "content/format/ManifestWriter.hpp"

#include <stdexcept>

#include "content/crypto/Aead.hpp"
#include "content/crypto/Hasher.hpp"
#include "content/format/BinaryWriter.hpp"
#include "content/format/Catalog.hpp"
#include "content/format/ShardHeader.hpp"

namespace haylen::content {

std::vector<std::uint8_t> ManifestWriter::writeContext(const Manifest::Envelope& fields, std::uint32_t envelopeSize) {
    BinaryWriter writer;
    writer.writeBytes(Manifest::kMagic).write(Manifest::kVersion).writeZeros(sizeof(std::uint16_t)).write(envelopeSize);
    writer.write(static_cast<std::uint8_t>(fields.domain)).writeZeros(1).write(ShardHeader::kVersion).write(Catalog::kVersion).writeZeros(sizeof(std::uint16_t));
    writer.writeDigest(fields.application).writeDigest(fields.signingKeyId).writeDigest(fields.contentKeyId);
    writer.write(fields.generation).writeDigest(fields.previousManifest).write(fields.minimumAppBuild).write(fields.maximumAppBuild);
    writer.writeString(fields.profile).writeString(fields.channel).writeString(fields.luaAbi).write(std::uint64_t{fields.shards.size()});
    for (const ShardReference& shard : fields.shards) {
        writer.writeDigest(shard.shardId).write(shard.fileSize).writeDigest(shard.fileDigest);
    }
    return writer.take();
}

std::vector<std::uint8_t> ManifestWriter::write(Manifest::Envelope fields, std::span<const std::uint8_t> catalog, const ContentKey& contentKey, const SigningKey& signingKey) {
    if (fields.profile.empty() || !Manifest::isValidName(fields.profile) || !Manifest::isValidName(fields.channel) || !Manifest::isValidName(fields.luaAbi)) {
        throw std::invalid_argument("A manifest needs a profile, and its profile, channel and Lua ABI use at most 64 lowercase letters, digits, dashes and dots.");
    }
    if (fields.minimumAppBuild > fields.maximumAppBuild || fields.shards.size() > Manifest::kMaximumShards || catalog.size() > Manifest::kMaximumCatalogSize) {
        throw std::invalid_argument("A manifest needs a build range from low to high, and it holds at most its limits of shards and catalog bytes.");
    }
    fields.contentKeyId = contentKey.getId();
    fields.signingKeyId = signingKey.getVerifyingKey().getId();

    // The size of the envelope is part of the context, and the catalog fields after the context have a fixed size.
    const auto envelopeSize = static_cast<std::uint32_t>(writeContext(fields, 0).size() + Manifest::kCatalogFieldsSize);
    const std::vector<std::uint8_t> context = writeContext(fields, envelopeSize);

    std::vector<std::uint8_t> encrypted(catalog.begin(), catalog.end());
    const Aead::Nonce nonce = contentKey.deriveNonce(ContentKey::Purpose::Catalog, Hasher().update(context).update(catalog).finish().getBytes());
    const Aead::Tag tag = Aead::seal(contentKey.getSubkey(ContentKey::Purpose::Catalog), nonce, context, encrypted, encrypted);

    BinaryWriter writer;
    writer.writeBytes(context).write(std::uint64_t{encrypted.size()}).writeDigest(Digest::of(encrypted)).writeBytes(nonce).writeBytes(tag);
    const VerifyingKey::Signature signature = signingKey.sign(writer.getBytes());
    writer.writeBytes(signature).writeBytes(encrypted);
    return writer.take();
}

} // namespace haylen::content
