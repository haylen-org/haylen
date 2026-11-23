#include "content/format/BinaryReader.hpp"

#include <algorithm>

namespace haylen::content {

BinaryReader::BinaryReader(std::span<const std::uint8_t> source, Error::Code errorCode, std::string_view subject) noexcept : bytes(source), code(errorCode), name(subject) {}

std::span<const std::uint8_t> BinaryReader::readBytes(std::size_t count) {
    if (count > getRemaining()) {
        fail();
    }

    const std::span<const std::uint8_t> field = bytes.subspan(position, count);
    position += count;
    return field;
}

Digest BinaryReader::readDigest() {
    return Digest(readBytes(Digest::kSize).first<Digest::kSize>());
}

std::string_view BinaryReader::readString(std::size_t maximum) {
    const std::size_t length = read<std::uint16_t>();
    if (length > maximum) {
        fail();
    }

    const std::span<const std::uint8_t> text = readBytes(length);
    return {reinterpret_cast<const char*>(text.data()), text.size()};
}

void BinaryReader::skip(std::size_t count) {
    if (count > getRemaining()) {
        fail();
    }

    position += count;
}

void BinaryReader::skipZeros(std::size_t count) {
    const std::span<const std::uint8_t> reserved = readBytes(count);
    if (!std::ranges::all_of(reserved, [](std::uint8_t byte) { return byte == 0; })) {
        fail();
    }
}

void BinaryReader::requireEnd() const {
    if (position != bytes.size()) {
        fail();
    }
}

void BinaryReader::fail() const {
    throw Error(code, describeDamage(name));
}

std::string BinaryReader::describeDamage(std::string_view subject) {
    return std::string(subject) + " is damaged or was not made by this version of Haylen.";
}

} // namespace haylen::content
