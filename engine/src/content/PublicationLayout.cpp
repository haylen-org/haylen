#include "content/PublicationLayout.hpp"

namespace haylen::content {

std::string PublicationLayout::getPackPath(const ShardReference& pack) {
    const std::string name = pack.getFileName();
    return std::string(kPacksFolder) + "/" + name.substr(0, 2) + "/" + name;
}

std::string PublicationLayout::getManifestPath(const Digest& manifest) {
    return std::string(kManifestsFolder) + "/" + manifest.toHex() + ".hmanifest";
}

std::string PublicationLayout::getChannelPath(std::string_view channel) {
    return std::string(kChannelsFolder) + "/" + std::string(channel) + ".hchannel";
}

} // namespace haylen::content
