#include "content/format/ChannelDescriptor.hpp"

#include <algorithm>
#include <format>

#include "content/Error.hpp"
#include "content/format/BinaryReader.hpp"
#include "content/format/BinaryWriter.hpp"

namespace haylen::content {

std::vector<std::uint8_t> ChannelDescriptor::write(const SigningKey& signingKey) const {
    BinaryWriter writer;
    writer.writeBytes(kMagic).write(kVersion).writeZeros(sizeof(std::uint16_t));
    writer.writeDigest(application).writeDigest(signingKey.getVerifyingKey().getId()).write(generation).writeDigest(manifest).writeString(channel);
    const VerifyingKey::Signature signature = signingKey.sign(writer.getBytes());
    writer.writeBytes(signature);
    return writer.take();
}

ChannelDescriptor ChannelDescriptor::read(std::span<const std::uint8_t> bytes, std::span<const VerifyingKey> trustedKeys) {
    BinaryReader reader(bytes, Error::Code::CorruptManifest, "channel descriptor");
    if (bytes.size() < kSigningKeyOffset + Digest::kSize + VerifyingKey::kSignatureSize) {
        reader.fail("ends before its fields and signature do");
    }
    if (!std::ranges::equal(reader.readBytes(kMagic.size()), kMagic)) {
        throw Error(Error::Code::UnsupportedFormat, "The file is not a Haylen channel descriptor.");
    }
    if (reader.read<std::uint16_t>() != kVersion) {
        throw Error(Error::Code::UnsupportedVersion, "The channel descriptor has a version this app does not read.");
    }

    const std::span<const std::uint8_t> signedBytes = bytes.first(bytes.size() - VerifyingKey::kSignatureSize);
    const Digest keyId(signedBytes.subspan(kSigningKeyOffset).first<Digest::kSize>());
    const auto trusted = std::ranges::find(trustedKeys, keyId, &VerifyingKey::getId);
    if (trusted == trustedKeys.end()) {
        throw Error(Error::Code::ManifestSignatureInvalid, "The channel descriptor is signed by the key \"" + keyId.toHex() + "\", which this app does not trust.");
    }
    VerifyingKey::Signature signature{};
    std::ranges::copy(bytes.last(VerifyingKey::kSignatureSize), signature.begin());
    if (!trusted->verify(signedBytes, signature)) {
        throw Error(Error::Code::ManifestSignatureInvalid, "The signature of the channel descriptor is not valid, so it is damaged or was changed.");
    }

    BinaryReader fields(signedBytes, Error::Code::CorruptManifest, "channel descriptor");
    fields.skip(kMagic.size() + sizeof(std::uint16_t));
    fields.skipZeros(sizeof(std::uint16_t));
    ChannelDescriptor descriptor;
    descriptor.application = fields.readDigest();
    descriptor.signingKeyId = fields.readDigest();
    descriptor.generation = fields.read<std::uint64_t>();
    descriptor.manifest = fields.readDigest();
    descriptor.channel = fields.readString(Manifest::kMaximumNameLength);
    fields.requireEnd();
    return descriptor;
}

bool ChannelDescriptor::offersUpdate(std::uint64_t acceptedGeneration, const Digest& acceptedManifest) const {
    if (generation > acceptedGeneration) {
        return true;
    }
    if (generation == acceptedGeneration && manifest == acceptedManifest) {
        return false;
    }
    throw Error(Error::Code::ManifestRollbackRejected, std::format("The channel \"{}\" offers generation {} with manifest \"{}\", but this app already accepted generation {} with manifest \"{}\".", channel, generation, manifest.toHex(), acceptedGeneration, acceptedManifest.toHex()));
}

bool ChannelDescriptor::describes(const Manifest& candidate) const noexcept {
    const Manifest::Envelope& envelope = candidate.getEnvelope();
    return candidate.getId() == manifest && envelope.domain == Manifest::Domain::Content && envelope.application == application && envelope.channel == channel && envelope.generation == generation;
}

} // namespace haylen::content
