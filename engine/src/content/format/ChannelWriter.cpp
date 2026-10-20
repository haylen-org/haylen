#include "content/format/ChannelWriter.hpp"

#include "content/format/BinaryWriter.hpp"

namespace haylen::content {

std::vector<std::uint8_t> ChannelWriter::write(const ChannelDescriptor& descriptor, const SigningKey& signingKey) {
    BinaryWriter writer;
    writer.writeBytes(ChannelDescriptor::kMagic).write(ChannelDescriptor::kVersion).writeZeros(sizeof(std::uint16_t));
    writer.writeDigest(descriptor.application).writeDigest(signingKey.getVerifyingKey().getId()).write(descriptor.generation).writeDigest(descriptor.manifest).writeString(descriptor.channel);
    const VerifyingKey::Signature signature = signingKey.sign(writer.getBytes());
    writer.writeBytes(signature);
    return writer.take();
}

} // namespace haylen::content
