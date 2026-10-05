#include "content/crypto/Hasher.hpp"

#include <array>

namespace haylen::content {

Hasher::Hasher(std::size_t outputSize, std::span<const std::uint8_t> key) noexcept {
    crypto_blake2b_keyed_init(&context, outputSize, key.data(), key.size());
}

Hasher::~Hasher() {
    crypto_wipe(&context, sizeof(context));
}

Hasher& Hasher::update(std::span<const std::uint8_t> bytes) noexcept {
    crypto_blake2b_update(&context, bytes.data(), bytes.size());
    return *this;
}

Hasher& Hasher::updateLabel(std::string_view label) noexcept {
    const std::uint8_t length = static_cast<std::uint8_t>(label.size());
    crypto_blake2b_update(&context, &length, 1);
    crypto_blake2b_update(&context, reinterpret_cast<const std::uint8_t*>(label.data()), label.size());
    return *this;
}

Hasher& Hasher::update(const Digest& digest) noexcept {
    return update(digest.getBytes());
}

void Hasher::finish(std::span<std::uint8_t> output) noexcept {
    crypto_blake2b_final(&context, output.data());
}

Digest Hasher::finish() noexcept {
    std::array<std::uint8_t, Digest::kSize> bytes{};
    finish(bytes);
    return Digest(bytes);
}

} // namespace haylen::content
