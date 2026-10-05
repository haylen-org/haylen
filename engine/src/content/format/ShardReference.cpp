#include "content/format/ShardReference.hpp"

namespace haylen::content {

std::string ShardReference::getFileName() const {
    return shardId.toHex() + ".hpak";
}

} // namespace haylen::content
