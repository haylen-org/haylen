#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "content/crypto/VerifyingKey.hpp"

namespace haylen::content {

// The private Ed25519 key that signs manifests, which exists only where content is published. It is wiped when it ends.
class SigningKey final {
  public:
    static constexpr std::size_t kSeedSize = 32;

    explicit SigningKey(std::span<const std::uint8_t, kSeedSize> seed) noexcept;
    ~SigningKey();

    SigningKey(const SigningKey&) = delete;
    SigningKey& operator=(const SigningKey&) = delete;

    [[nodiscard]] const VerifyingKey& getVerifyingKey() const noexcept {
        return verifyingKey;
    }
    [[nodiscard]] VerifyingKey::Signature sign(std::span<const std::uint8_t> message) const noexcept;

  private:
    static constexpr std::size_t kSecretSize = 64;

    [[nodiscard]] static VerifyingKey derive(std::span<const std::uint8_t, kSeedSize> seed, std::array<std::uint8_t, kSecretSize>& secret) noexcept;

    std::array<std::uint8_t, kSecretSize> secretKey{};
    VerifyingKey verifyingKey;
};

} // namespace haylen::content
