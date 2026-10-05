#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "content/Compatibility.hpp"
#include "content/ContentBuilder.hpp"
#include "content/crypto/KeyRing.hpp"
#include "content/crypto/SigningKey.hpp"
#include "content/format/Manifest.hpp"
#include "haylen/io/Package.hpp"
#include "support/TemporaryDirectory.hpp"

namespace haylen::test {

// Builds the protected release of an app package into a temporary folder with fixed test keys, the way a release build does, and opens it again.
class ReleaseFixture final {
  public:
    static constexpr std::string_view kIdentifier = "dev.haylen.tests";
    static constexpr std::string_view kProfile = "desktop";
    static constexpr std::uint64_t kAppBuild = 7;

    ReleaseFixture();

    // Builds the app domain from `app.json`, `source` and `plugins` and the content domain from `content`. A later build reuses the chunks of the one before it.
    void build(const io::Package& source, std::uint64_t shardTarget = content::ContentBuilder::kDefaultShardTarget);
    void build(const std::map<std::string, std::string>& files, std::uint64_t shardTarget = content::ContentBuilder::kDefaultShardTarget);

    [[nodiscard]] std::unique_ptr<io::Package> open(std::shared_ptr<const io::Package> files) const;
    [[nodiscard]] std::unique_ptr<io::Package> open() const;
    [[nodiscard]] std::shared_ptr<const io::Package> getFiles() const;

    [[nodiscard]] const std::filesystem::path& getFolder() const noexcept {
        return directory.getPath();
    }
    [[nodiscard]] const std::shared_ptr<content::KeyRing>& getKeys() const noexcept {
        return keys;
    }
    [[nodiscard]] const content::SigningKey& getSigningKey() const noexcept {
        return signingKey;
    }
    [[nodiscard]] std::shared_ptr<const content::ContentKey> getContentKey() const;
    [[nodiscard]] content::Compatibility getCompatibility() const;
    [[nodiscard]] const content::ContentBuilder::Statistics& getStatistics(content::Manifest::Domain domain) const;

    [[nodiscard]] static std::array<std::uint8_t, 32> makeKey(std::uint8_t first) noexcept;

  private:
    struct Domain {
        std::string manifestFile;
        std::unique_ptr<content::Catalog> catalog;
        std::vector<content::ShardReference> shards;
        content::Digest manifestId;
        content::ContentBuilder::Statistics statistics;
    };

    void buildDomain(const io::Package& source, content::Manifest::Domain domain, std::uint64_t shardTarget);

    TemporaryDirectory directory;
    std::shared_ptr<content::KeyRing> keys = std::make_shared<content::KeyRing>();
    content::Digest contentKeyId;
    content::SigningKey signingKey;
    std::uint64_t generation = 0;
    std::map<content::Manifest::Domain, Domain> domains;
};

} // namespace haylen::test
