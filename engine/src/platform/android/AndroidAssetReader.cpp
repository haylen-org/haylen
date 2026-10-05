#include "platform/android/AndroidAssetReader.hpp"

#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <utility>

namespace haylen::platform {

AndroidAssetReader::AndroidAssetReader(Handle openAsset) : asset(std::move(openAsset)), size(static_cast<std::uint64_t>(AAsset_getLength64(asset.get()))) {}

std::size_t AndroidAssetReader::read(std::uint64_t offset, std::span<std::uint8_t> target) {
    if (offset >= size || target.empty()) {
        return 0;
    }

    const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(target.size(), size - offset));
    std::scoped_lock lock(mutex);
    if (AAsset_seek64(asset.get(), static_cast<off64_t>(offset), SEEK_SET) < 0) {
        throw std::runtime_error("An APK asset could not be read at the offset asked for.");
    }

    std::size_t filled = 0;
    while (filled < count) {
        const int received = AAsset_read(asset.get(), target.data() + filled, count - filled);
        if (received <= 0) {
            throw std::runtime_error("An APK asset could not be read to the end.");
        }
        filled += static_cast<std::size_t>(received);
    }
    return count;
}

} // namespace haylen::platform
