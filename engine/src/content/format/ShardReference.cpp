#include "content/format/ShardReference.hpp"

namespace haylen::content {

std::string ShardReference::getFileName() const {
    return shardId.toHex() + std::string(kExtension);
}

} // namespace haylen::content
