#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "content/crypto/ContentKey.hpp"
#include "content/crypto/Digest.hpp"
#include "content/crypto/KeyRing.hpp"
#include "content/crypto/SigningKey.hpp"

namespace haylen::content {

// The keys of one app on the machines that build its releases: its content keys, the last of which encrypts new content, and the seed of its signing key, in a folder that only its owner reads. The folder lives outside every repository and build folder, and continuous integration receives a copy of it as a secret. Keys are never printed, and only their IDs and the public key leave the folder.
class KeyStore final {
  public:
    static constexpr std::string_view kIndexFile = "keys.json";
    static constexpr std::string_view kSigningKeyFile = "signing.key";

    // Creates the keys of an app in a folder that holds no keys yet: one content key and one signing key from the random generator of the system. Throws `std::invalid_argument` when the folder holds keys already.
    [[nodiscard]] static KeyStore create(const std::filesystem::path& folder, std::string identifier);

    // Reads the keys of a folder, and throws `std::runtime_error` when a file is missing, damaged or holds another key than the index names.
    [[nodiscard]] static KeyStore open(const std::filesystem::path& folder);

    // Adds a new content key, which encrypts the content of later builds, and keeps the earlier ones, which installed content still needs.
    void rotate();

    [[nodiscard]] const std::string& getIdentifier() const noexcept {
        return identifier;
    }
    [[nodiscard]] std::shared_ptr<const KeyRing> getContentKeys() const noexcept {
        return ring;
    }
    [[nodiscard]] const std::vector<Digest>& getContentKeyIds() const noexcept {
        return contentKeyIds;
    }
    [[nodiscard]] const Digest& getActiveKeyId() const noexcept {
        return contentKeyIds.back();
    }
    [[nodiscard]] const std::shared_ptr<const SigningKey>& getSigningKey() const noexcept {
        return signingKey;
    }

  private:
    using Secret = std::array<std::uint8_t, ContentKey::kSize>;

    KeyStore(std::filesystem::path keyFolder, std::string appIdentifier);

    [[nodiscard]] static std::string getContentKeyFile(const Digest& id);
    [[nodiscard]] static Secret readSecret(const std::filesystem::path& file);
    static void writePrivate(const std::filesystem::path& file, std::span<const std::uint8_t> bytes);
    void writeIndex() const;

    std::filesystem::path folder;
    std::string identifier;
    std::shared_ptr<KeyRing> ring = std::make_shared<KeyRing>();
    std::vector<Digest> contentKeyIds;
    std::shared_ptr<const SigningKey> signingKey;
};

} // namespace haylen::content
