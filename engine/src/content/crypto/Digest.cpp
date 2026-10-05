#include "content/crypto/Digest.hpp"

#include <monocypher.h>

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "content/crypto/Hasher.hpp"

namespace haylen::content {

Digest::Digest(std::span<const std::uint8_t, kSize> source) noexcept {
    std::copy(source.begin(), source.end(), bytes.begin());
}

Digest Digest::of(std::span<const std::uint8_t> message) noexcept {
    Digest digest;
    crypto_blake2b(digest.bytes.data(), kSize, message.data(), message.size());
    return digest;
}

Digest Digest::ofFile(const std::filesystem::path& file) {
    std::ifstream stream(file, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("The file \"" + file.generic_string() + "\" could not be read.");
    }
    std::vector<std::uint8_t> buffer(kFileBlockSize);
    Hasher hasher;
    while (stream) {
        stream.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
        hasher.update(std::span(buffer).first(static_cast<std::size_t>(stream.gcount())));
    }
    if (stream.bad()) {
        throw std::runtime_error("The file \"" + file.generic_string() + "\" could not be read to its end.");
    }
    return hasher.finish();
}

bool Digest::isZero() const noexcept {
    return std::ranges::all_of(bytes, [](std::uint8_t byte) { return byte == 0; });
}

std::optional<Digest> Digest::fromHex(std::string_view text) noexcept {
    if (text.size() != kSize * 2) {
        return std::nullopt;
    }
    Digest digest;
    for (std::size_t index = 0; index < text.size(); ++index) {
        const std::size_t value = kDigits.find(text[index]);
        if (value == std::string_view::npos) {
            return std::nullopt;
        }
        digest.bytes[index / 2] = static_cast<std::uint8_t>(digest.bytes[index / 2] | (index % 2 == 0 ? value << 4 : value));
    }
    return digest;
}

std::string Digest::toHex() const {
    std::string text;
    text.reserve(kSize * 2);
    for (const std::uint8_t byte : bytes) {
        text.push_back(kDigits[byte >> 4]);
        text.push_back(kDigits[byte & 0x0F]);
    }
    return text;
}

std::size_t Digest::Hash::operator()(const Digest& digest) const noexcept {
    // The bytes of a digest are already uniformly distributed, so its first eight make a good hash.
    std::size_t value = 0;
    for (std::size_t index = 0; index < sizeof(std::size_t); ++index) {
        value = (value << 8) | digest.bytes[index];
    }
    return value;
}

} // namespace haylen::content
