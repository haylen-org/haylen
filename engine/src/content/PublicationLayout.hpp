#pragma once

#include <string>
#include <string_view>

#include "content/crypto/Digest.hpp"
#include "content/format/ShardReference.hpp"

namespace haylen::content {

// Where the files of a publication tree lie, relative to its root, which the publisher writes and the updater of an app downloads: `channels/<channel>.hchannel`, the signed pointer of a channel, `manifests/<id>.hmanifest` and `packs/<ab>/<id>.hpak`. Every name is an opaque ID or the name of a channel, so no name tells what a file holds.
class PublicationLayout final {
  public:
    static constexpr std::string_view kChannelsFolder = "channels";
    static constexpr std::string_view kManifestsFolder = "manifests";
    static constexpr std::string_view kPacksFolder = "packs";

    [[nodiscard]] static std::string getPackPath(const ShardReference& pack);
    [[nodiscard]] static std::string getManifestPath(const Digest& manifest);
    [[nodiscard]] static std::string getChannelPath(std::string_view channel);
};

} // namespace haylen::content
