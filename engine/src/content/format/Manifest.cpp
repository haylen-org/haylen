#include "content/format/Manifest.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "content/Error.hpp"
#include "content/crypto/Hasher.hpp"
#include "content/format/BinaryReader.hpp"
#include "content/format/BinaryWriter.hpp"
#include "content/format/ShardHeader.hpp"
#include "haylen/io/Path.hpp"

namespace haylen::content {

Digest Manifest::identifyApplication(std::string_view identifier) noexcept {
    return Hasher().updateLabel("haylen/hpak/v1/application").update({reinterpret_cast<const std::uint8_t*>(identifier.data()), identifier.size()}).finish();
}

bool Manifest::isValidName(std::string_view name) noexcept {
    return name.size() <= kMaximumNameLength && std::ranges::all_of(name, [](char character) { return (character >= 'a' && character <= 'z') || (character >= '0' && character <= '9') || character == '-' || character == '.'; });
}

bool Manifest::belongsToDomain(std::string_view path, Domain domain) noexcept {
    if (domain == Domain::Content) {
        return io::Path::isInside(path, io::Path::kContentDirectory);
    }
    return path == io::Path::kAppConfigFile || io::Path::isInside(path, io::Path::kSourceDirectory) || io::Path::isInside(path, io::Path::kPluginsDirectory);
}

std::vector<std::uint8_t> Manifest::writeContext(const Envelope& fields, std::uint32_t envelopeSize) {
    BinaryWriter writer;
    writer.writeBytes(kMagic).write(kVersion).writeZeros(sizeof(std::uint16_t)).write(envelopeSize);
    writer.write(static_cast<std::uint8_t>(fields.domain)).writeZeros(1).write(ShardHeader::kVersion).write(Catalog::kVersion).writeZeros(sizeof(std::uint16_t));
    writer.writeDigest(fields.application).writeDigest(fields.signingKeyId).writeDigest(fields.contentKeyId);
    writer.write(fields.generation).writeDigest(fields.previousManifest).write(fields.minimumAppBuild).write(fields.maximumAppBuild);
    writer.writeString(fields.profile).writeString(fields.channel).write(std::uint64_t{fields.shards.size()});
    for (const ShardReference& shard : fields.shards) {
        writer.writeDigest(shard.shardId).write(shard.fileSize).writeDigest(shard.fileDigest);
    }
    return writer.take();
}

std::vector<std::uint8_t> Manifest::write(Envelope fields, std::span<const std::uint8_t> catalog, const ContentKey& contentKey, const SigningKey& signingKey) {
    if (fields.profile.empty() || !isValidName(fields.profile) || !isValidName(fields.channel)) {
        throw std::invalid_argument("A manifest needs a profile, and its profile and channel use at most 64 lowercase letters, digits, dashes and dots.");
    }
    if (fields.minimumAppBuild > fields.maximumAppBuild || fields.shards.size() > kMaximumShards || catalog.size() > kMaximumCatalogSize) {
        throw std::invalid_argument("A manifest needs a build range from low to high, and it holds at most its limits of shards and catalog bytes.");
    }
    fields.contentKeyId = contentKey.getId();
    fields.signingKeyId = signingKey.getVerifyingKey().getId();

    // The size of the envelope is part of the context, and the catalog fields after the context have a fixed size.
    const auto envelopeSize = static_cast<std::uint32_t>(writeContext(fields, 0).size() + kCatalogFieldsSize);
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

Manifest Manifest::read(std::vector<std::uint8_t> file, std::span<const VerifyingKey> trustedKeys) {
    BinaryReader prefix(file, Error::Code::CorruptManifest, "manifest");
    if (file.size() < kPrefixSize + VerifyingKey::kSignatureSize) {
        prefix.fail("ends before its envelope and signature do");
    }
    if (!std::ranges::equal(prefix.readBytes(kMagic.size()), kMagic)) {
        throw Error(Error::Code::UnsupportedFormat, "The file is not a Haylen manifest.");
    }
    if (prefix.read<std::uint16_t>() != kVersion) {
        throw Error(Error::Code::UnsupportedVersion, "The manifest has a version this app does not read.");
    }
    prefix.skipZeros(sizeof(std::uint16_t));

    // Only the size of the envelope is trusted before the signature, and only to find the signature.
    const auto envelopeSize = prefix.read<std::uint32_t>();
    if (envelopeSize < kSigningKeyOffset + Digest::kSize || envelopeSize > kMaximumEnvelopeSize || envelopeSize > file.size() - VerifyingKey::kSignatureSize) {
        prefix.fail("declares an envelope that does not fit in it");
    }
    const std::span<const std::uint8_t> signedBytes = std::span(file).first(envelopeSize);
    const Digest signingKeyId(signedBytes.subspan(kSigningKeyOffset).first<Digest::kSize>());
    const auto trusted = std::ranges::find(trustedKeys, signingKeyId, &VerifyingKey::getId);
    if (trusted == trustedKeys.end()) {
        throw Error(Error::Code::ManifestSignatureInvalid, "The manifest is signed by the key \"" + signingKeyId.toHex() + "\", which this app does not trust.");
    }
    VerifyingKey::Signature signature{};
    std::ranges::copy(std::span(file).subspan(envelopeSize, VerifyingKey::kSignatureSize), signature.begin());
    if (!trusted->verify(signedBytes, signature)) {
        throw Error(Error::Code::ManifestSignatureInvalid, "The signature of the manifest is not valid, so the manifest is damaged or was changed.");
    }

    Manifest manifest;
    manifest.parseEnvelope(signedBytes);
    manifest.id = Hasher().updateLabel("haylen/hpak/v1/manifest-id").update(signedBytes).finish();
    if (manifest.catalogSize != file.size() - envelopeSize - VerifyingKey::kSignatureSize) {
        prefix.fail("holds a catalog of another size than its envelope signs");
    }
    manifest.bytes = std::move(file);
    return manifest;
}

void Manifest::parseEnvelope(std::span<const std::uint8_t> signedBytes) {
    BinaryReader reader(signedBytes, Error::Code::CorruptManifest, "manifest envelope");
    reader.skip(kPrefixSize);

    const auto domain = reader.read<std::uint8_t>();
    if (domain > static_cast<std::uint8_t>(Domain::Content)) {
        reader.fail("names a domain this app does not know");
    }
    envelope.domain = static_cast<Domain>(domain);
    reader.skipZeros(1);
    if (reader.read<std::uint16_t>() != ShardHeader::kVersion || reader.read<std::uint16_t>() != Catalog::kVersion) {
        throw Error(Error::Code::UnsupportedVersion, "The manifest uses shard or catalog versions this app does not read.");
    }
    reader.skipZeros(sizeof(std::uint16_t));

    envelope.application = reader.readDigest();
    envelope.signingKeyId = reader.readDigest();
    envelope.contentKeyId = reader.readDigest();
    envelope.generation = reader.read<std::uint64_t>();
    envelope.previousManifest = reader.readDigest();
    envelope.minimumAppBuild = reader.read<std::uint64_t>();
    envelope.maximumAppBuild = reader.read<std::uint64_t>();
    envelope.profile = reader.readString(kMaximumNameLength);
    envelope.channel = reader.readString(kMaximumNameLength);
    if (envelope.profile.empty() || !isValidName(envelope.profile) || !isValidName(envelope.channel) || envelope.minimumAppBuild > envelope.maximumAppBuild) {
        reader.fail("holds a profile, a channel or a build range that is not valid");
    }

    const auto shardCount = reader.read<std::uint64_t>();
    if (shardCount > kMaximumShards || shardCount > reader.getRemaining() / kShardReferenceSize) {
        reader.fail("lists more shards than it holds");
    }
    envelope.shards.reserve(static_cast<std::size_t>(shardCount));
    for (std::uint64_t index = 0; index < shardCount; ++index) {
        ShardReference shard;
        shard.shardId = reader.readDigest();
        shard.fileSize = reader.read<std::uint64_t>();
        shard.fileDigest = reader.readDigest();
        envelope.shards.push_back(shard);
    }
    contextSize = reader.getPosition();

    catalogSize = reader.read<std::uint64_t>();
    catalogDigest = reader.readDigest();
    std::ranges::copy(reader.readBytes(Aead::kNonceSize), catalogNonce.begin());
    std::ranges::copy(reader.readBytes(Aead::kTagSize), catalogTag.begin());
    reader.requireEnd();
    if (catalogSize > kMaximumCatalogSize) {
        reader.fail("declares a catalog larger than the limit");
    }
}

Catalog Manifest::decryptCatalog(const KeyRing& keys) const {
    const std::shared_ptr<const ContentKey> key = keys.get(envelope.contentKeyId);
    const std::size_t catalogOffset = bytes.size() - static_cast<std::size_t>(catalogSize);
    const std::span<const std::uint8_t> encrypted = std::span(bytes).subspan(catalogOffset);
    std::vector<std::uint8_t> plain(encrypted.begin(), encrypted.end());
    if (Digest::of(encrypted) != catalogDigest || !Aead::open(key->getSubkey(ContentKey::Purpose::Catalog), catalogNonce, std::span(bytes).first(contextSize), catalogTag, plain, plain)) {
        throw Error(Error::Code::CatalogAuthenticationFailed, "The catalog of the manifest \"" + id.toHex() + "\" failed authentication, so the manifest is damaged or was changed.");
    }

    Catalog catalog = Catalog::parse(std::move(plain), envelope.shards.size());
    for (std::uint64_t index = 0; index < catalog.getFileCount(); ++index) {
        const Catalog::File file = catalog.getFile(index);
        if (!belongsToDomain(file.path, envelope.domain) || (envelope.domain == Domain::App && file.delivery != Delivery::Required)) {
            throw Error(Error::Code::CorruptCatalog, "The catalog of the manifest \"" + id.toHex() + "\" holds a file that its domain does not allow.");
        }
    }
    return catalog;
}

} // namespace haylen::content
