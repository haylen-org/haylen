#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <string_view>

#include "content/Compatibility.hpp"
#include "content/ContentBuilder.hpp"
#include "content/ReleaseBuilder.hpp"
#include "content/crypto/KeyRing.hpp"
#include "content/crypto/SigningKey.hpp"
#include "content/format/Manifest.hpp"
#include "haylen/io/Package.hpp"
#include "support/TemporaryDirectory.hpp"

namespace haylen::test {

// Builds the protected release of an app package with fixed test keys through the release builder, each build into a folder of its own that reuses the release before it, and opens it again.
class ReleaseFixture final {
  public:
    static constexpr std::string_view kIdentifier = "dev.haylen.tests";
    static constexpr std::string_view kProfile = "desktop";
    static constexpr std::uint64_t kAppBuild = 7;

    ReleaseFixture();

    void build(const io::Package& source, std::uint64_t shardTarget = content::ContentBuilder::kDefaultShardTarget);
    void build(const std::map<std::string, std::string>& files, std::uint64_t shardTarget = content::ContentBuilder::kDefaultShardTarget);

    [[nodiscard]] std::unique_ptr<io::Package> open(std::shared_ptr<const io::Package> files) const;
    [[nodiscard]] std::unique_ptr<io::Package> open() const;
    [[nodiscard]] std::shared_ptr<const io::Package> getFiles() const;

    // The folder of the last build.
    [[nodiscard]] const std::filesystem::path& getFolder() const noexcept {
        return folder;
    }
    [[nodiscard]] const std::shared_ptr<content::KeyRing>& getKeys() const noexcept {
        return keys;
    }
    [[nodiscard]] const content::SigningKey& getSigningKey() const noexcept {
        return *signingKey;
    }
    [[nodiscard]] std::shared_ptr<const content::ContentKey> getContentKey() const;
    [[nodiscard]] content::Compatibility getCompatibility() const;
    [[nodiscard]] const content::ContentBuilder::Statistics& getStatistics(content::Manifest::Domain domain) const;
    [[nodiscard]] content::ReleaseBuilder makeBuilder() const;

    [[nodiscard]] static std::array<std::uint8_t, 32> makeKey(std::uint8_t first) noexcept;

  private:
    TemporaryDirectory directory;
    std::shared_ptr<content::KeyRing> keys = std::make_shared<content::KeyRing>();
    content::Digest contentKeyId;
    std::shared_ptr<const content::SigningKey> signingKey;
    int builds = 0;
    std::filesystem::path folder;
    content::ReleaseBuilder::Result result;
};

} // namespace haylen::test
