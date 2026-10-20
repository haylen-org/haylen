#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "content/ContentBuilder.hpp"
#include "content/RecordCache.hpp"
#include "content/crypto/Digest.hpp"
#include "content/crypto/KeyRing.hpp"
#include "content/crypto/SigningKey.hpp"
#include "content/format/Manifest.hpp"
#include "content/format/ShardReference.hpp"
#include "haylen/core/AppConfig.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "haylen/io/Package.hpp"

namespace haylen::content {

// Builds the protected release of an app package into a folder of its own: the app domain from `app.json`, the Lua modules under `source`, compiled into bytecode, and the manifest and Lua modules of every plugin the app lists, and the content domain from the assets under `content`, each into its own shards and signed manifest. Nothing else of the package folder takes part, so the platform projects, notes and build files next to it never reach a release, and a file can never move from one domain to the other. A build that follows an earlier release of the app reuses its shards the way `ContentBuilder` does, and links or copies the shards it keeps into the new folder, so unchanged content keeps its shard files byte for byte. The earlier release may lie anywhere under its folder, as in the depots of a store, an unpacked app or a release folder, and only the shards the folder holds can be kept.
class ReleaseBuilder final {
  public:
    struct Options {
        std::string profile;
        std::uint64_t appBuild = 0;
        std::uint64_t generation = 1;
        std::uint64_t shardTarget = ContentBuilder::kDefaultShardTarget;
    };

    struct Domain {
        Digest manifest;
        std::vector<ShardReference> shards;
        std::size_t keptShards = 0;
        ContentBuilder::Statistics statistics;
    };

    struct Result {
        Domain app;
        Domain content;
    };

    // The shards of one domain and its signed manifest.
    struct DomainBuild {
        std::vector<std::uint8_t> manifest;
        ContentBuilder::Result result;
    };

    // New content is encrypted under the content key with the active ID, and the ring opens earlier releases under any of its keys.
    ReleaseBuilder(std::shared_ptr<const KeyRing> contentKeys, const Digest& activeKeyId, std::shared_ptr<const SigningKey> signing, std::shared_ptr<const RecordCache> recordCache = nullptr);

    // Builds the release of an app package into an output folder that does not exist yet, reusing the release under an earlier folder when one is given. Throws `std::invalid_argument` for an output folder that exists or an earlier folder that holds no release or two of them, and the errors of `AppConfig` for an `app.json` that is not valid.
    Result build(const io::Package& app, const std::filesystem::path& output, const std::optional<std::filesystem::path>& earlier, const Options& options) const;

    // The files of an earlier release by name: its manifests and the shard files at hand.
    using EarlierFiles = std::map<std::string, std::filesystem::path>;

    // Builds the shards of one domain into a folder and signs its manifest with the envelope, reusing the shards of an earlier manifest of the domain, of which the files hold the ones at hand, when one is given.
    [[nodiscard]] DomainBuild buildDomain(const io::Package& source, std::vector<ContentBuilder::Input> inputs, Manifest::Envelope envelope, const Manifest* earlier, const EarlierFiles& files, const std::filesystem::path& folder, std::uint64_t shardTarget) const;

    // Finds the manifests and the shard files of an earlier release anywhere under a folder. Throws `std::invalid_argument` for a folder that does not exist, holds no manifests of a release or holds a manifest twice.
    [[nodiscard]] static EarlierFiles findEarlierFiles(const std::filesystem::path& folder);

    // Reads a manifest of an earlier release that the signing key of this builder signed.
    [[nodiscard]] Manifest readManifest(const std::filesystem::path& file) const;

    // Lists the files of an app package that a domain holds, with the delivery that the rules of `app.json` give each asset, without the files that file managers leave in folders.
    [[nodiscard]] static std::vector<ContentBuilder::Input> listFiles(const io::Package& app, const core::AppConfig& config, Manifest::Domain domain);

  private:
    static constexpr std::string_view kFolderFile = ".DS_Store";

    [[nodiscard]] Domain buildRelease(const io::Package& app, const core::AppConfig& config, Manifest::Domain domain, const std::filesystem::path& output, const EarlierFiles& earlier, const Options& options) const;

    // Turns the Lua modules of the app domain into bytecode and marks them so, and keeps every other file as it is.
    [[nodiscard]] static std::unique_ptr<io::MemoryPackage> compileModules(const io::Package& app, std::vector<ContentBuilder::Input>& inputs);
    static void keepShard(const std::filesystem::path& source, const std::filesystem::path& output, const ShardReference& shard);

    std::shared_ptr<const KeyRing> keys;
    std::shared_ptr<const ContentKey> activeKey;
    std::shared_ptr<const SigningKey> signingKey;
    std::shared_ptr<const RecordCache> cache;
};

} // namespace haylen::content
