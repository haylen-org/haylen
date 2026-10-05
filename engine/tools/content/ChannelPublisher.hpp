#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "content/ContentBuilder.hpp"
#include "content/RecordCache.hpp"
#include "content/ReleaseBuilder.hpp"
#include "content/crypto/Digest.hpp"
#include "content/crypto/KeyRing.hpp"
#include "content/crypto/SigningKey.hpp"
#include "content/format/ChannelDescriptor.hpp"
#include "content/format/Manifest.hpp"
#include "content/format/ShardReference.hpp"
#include "haylen/io/Package.hpp"

namespace haylen::content {

// Publishes the content of an app to update channels as a tree of static files that any web server or content delivery network serves, all named by opaque IDs: `channels/<channel>.hchannel`, the signed pointer of a channel to its current content manifest, `manifests/<id>.hmanifest` and `packs/<ab>/<id>.hpak`. Manifests and packs are immutable, so a publish only adds files and stops before it would write different bytes under a name that exists, and the pointer moves last, after the new content verifies, so a publish that fails leaves every channel as it was. Only the content domain is published, since the code of an app belongs to the build that ships it.
class ChannelPublisher final {
  public:
    static constexpr std::string_view kChannelsFolder = "channels";
    static constexpr std::string_view kManifestsFolder = "manifests";
    static constexpr std::string_view kPacksFolder = "packs";

    struct Options {
        std::string profile;
        std::uint64_t appBuild = 0;
        std::string channel;
        std::uint64_t shardTarget = ContentBuilder::kDefaultShardTarget;
    };

    struct Publication {
        std::uint64_t generation = 0;
        Digest manifest;
        std::vector<ShardReference> newPacks;
        ContentBuilder::Statistics statistics;
    };

    struct Compaction {
        std::uint64_t manifests = 0;
        std::uint64_t packs = 0;
        std::uint64_t bytes = 0;
    };

    ChannelPublisher(std::shared_ptr<const KeyRing> contentKeys, const Digest& activeKeyId, std::shared_ptr<const SigningKey> signing, std::shared_ptr<const RecordCache> recordCache = nullptr);

    // Builds the content of an app as the next generation of a channel of the tree, reusing the packs of the current generation, and moves the channel to it. The content serves the given app build and every later one.
    Publication publish(const io::Package& app, const std::filesystem::path& tree, const Options& options) const;

    // Deletes the manifests that no channel reaches within its last `keep` generations, at least one, and the packs that no remaining manifest names.
    Compaction compact(const std::filesystem::path& tree, std::uint64_t keep) const;

    [[nodiscard]] static std::filesystem::path getPackPath(const ShardReference& pack);
    [[nodiscard]] static std::filesystem::path getManifestPath(const Digest& manifest);
    [[nodiscard]] static std::filesystem::path getChannelPath(std::string_view channel);

  private:
    static constexpr std::string_view kStagingFolder = "staging";

    [[nodiscard]] std::optional<ChannelDescriptor> readChannel(const std::filesystem::path& tree, std::string_view channel) const;
    [[nodiscard]] Manifest readManifest(const std::filesystem::path& tree, const Digest& manifest) const;
    void verify(const std::filesystem::path& tree, const Manifest& manifest) const;
    static void placeImmutable(const std::filesystem::path& source, const std::filesystem::path& destination);
    static void writeFile(const std::filesystem::path& file, std::span<const std::uint8_t> bytes);

    std::shared_ptr<const KeyRing> keys;
    std::shared_ptr<const SigningKey> signingKey;
    ReleaseBuilder builder;
};

} // namespace haylen::content
