#include "content/format/Chunker.hpp"

#include <algorithm>

namespace haylen::content {

std::size_t Chunker::findCut(std::span<const std::uint8_t> window) noexcept {
    static constexpr std::array<std::uint64_t, 256> kGear = makeGear();

    const std::size_t size = std::min(window.size(), kMaximumSize);
    if (size <= kMinimumSize) {
        return size;
    }

    const std::size_t target = std::min(size, kTargetSize);
    std::uint64_t hash = 0;
    std::size_t index = kMinimumSize;
    for (; index < target; ++index) {
        hash = (hash << 1) + kGear[window[index]];
        if ((hash & kSmallMask) == 0) {
            return index + 1;
        }
    }
    for (; index < size; ++index) {
        hash = (hash << 1) + kGear[window[index]];
        if ((hash & kLargeMask) == 0) {
            return index + 1;
        }
    }
    return size;
}

} // namespace haylen::content
