#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "content/crypto/Digest.hpp"

namespace haylen::content {

// The public Ed25519 key that checks the signatures of manifests, the only signing key material an app carries.
class VerifyingKey final {
  public:
    static constexpr std::size_t kSize = 32;
    static constexpr std::size_t kSignatureSize = 64;

    using Signature = std::array<std::uint8_t, kSignatureSize>;

    explicit VerifyingKey(std::span<const std::uint8_t, kSize> source) noexcept;

    [[nodiscard]] std::span<const std::uint8_t, kSize> getBytes() const noexcept {
        return bytes;
    }
    [[nodiscard]] const Digest& getId() const noexcept {
        return id;
    }
    [[nodiscard]] bool verify(std::span<const std::uint8_t> message, const Signature& signature) const noexcept;

  private:
    std::array<std::uint8_t, kSize> bytes{};
    Digest id;
};

} // namespace haylen::content
