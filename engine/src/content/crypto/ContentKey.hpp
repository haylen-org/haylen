#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "content/crypto/Aead.hpp"
#include "content/crypto/Digest.hpp"

namespace haylen::content {

// The master key that encrypts the content of one app, kept only as the subkeys derived from it for each purpose and as its public ID. The subkeys are wiped when the key ends.
class ContentKey final {
  public:
    static constexpr std::size_t kSize = 32;

    enum class Purpose : std::uint8_t {
        Chunk,
        ShardIndex,
        Catalog,
        Nonce,
    };

    explicit ContentKey(std::span<const std::uint8_t, kSize> master) noexcept;
    ~ContentKey();

    ContentKey(const ContentKey&) = delete;
    ContentKey& operator=(const ContentKey&) = delete;

    [[nodiscard]] const Digest& getId() const noexcept {
        return id;
    }
    [[nodiscard]] Aead::Key getSubkey(Purpose purpose) const noexcept;

    // Derives the nonce that encrypts a message for a purpose from everything that identifies the message, so the same message always encrypts to the same bytes and different messages never share a nonce.
    [[nodiscard]] Aead::Nonce deriveNonce(Purpose purpose, std::span<const std::uint8_t> identity) const noexcept;

  private:
    static constexpr std::string_view kIdLabel = "haylen/hpak/v1/key-id";
    static constexpr std::array<std::string_view, 4> kPurposeLabels = {"haylen/hpak/v1/chunk", "haylen/hpak/v1/shard-index", "haylen/hpak/v1/catalog", "haylen/hpak/v1/nonce"};

    [[nodiscard]] static std::string_view getLabel(Purpose purpose) noexcept;

    Digest id;
    std::array<std::array<std::uint8_t, kSize>, kPurposeLabels.size()> subkeys{};
};

} // namespace haylen::content
