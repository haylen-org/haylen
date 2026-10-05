#include "content/format/BinaryWriter.hpp"

#include <limits>
#include <stdexcept>

namespace haylen::content {

BinaryWriter& BinaryWriter::writeBytes(std::span<const std::uint8_t> data) {
    bytes.insert(bytes.end(), data.begin(), data.end());
    return *this;
}

BinaryWriter& BinaryWriter::writeDigest(const Digest& digest) {
    return writeBytes(digest.getBytes());
}

BinaryWriter& BinaryWriter::writeString(std::string_view text) {
    if (text.size() > std::numeric_limits<std::uint16_t>::max()) {
        throw std::length_error("A string of the content formats holds at most 65535 bytes.");
    }
    write(static_cast<std::uint16_t>(text.size()));
    bytes.insert(bytes.end(), text.begin(), text.end());
    return *this;
}

BinaryWriter& BinaryWriter::writeZeros(std::size_t count) {
    bytes.insert(bytes.end(), count, 0);
    return *this;
}

} // namespace haylen::content
