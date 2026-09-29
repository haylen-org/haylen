#include "2d/graphics/DrawItem.hpp"

#include <algorithm>
#include <bit>
#include <limits>

namespace haylen::graphics2d {

// Maps a float to an unsigned integer with the same order, so depths sort as plain integers.
std::uint32_t DrawItem::sortableFloat(float value) noexcept {
    const auto bits = std::bit_cast<std::uint32_t>(value);
    return (bits & 0x80000000U) != 0 ? ~bits : bits | 0x80000000U;
}

std::uint64_t DrawItem::makeKey(const DrawOrder& order, int layerOffset, Renderer::SortMode mode, float standingY) noexcept {
    constexpr std::int64_t kLowest = std::numeric_limits<std::int32_t>::min();
    constexpr std::int64_t kHighest = std::numeric_limits<std::int32_t>::max();
    const std::int64_t shifted = std::clamp(static_cast<std::int64_t>(order.layer) + layerOffset, kLowest, kHighest);
    const auto layer = static_cast<std::uint32_t>(shifted - kLowest);

    std::uint32_t inside = 0;
    if (mode == Renderer::SortMode::Depth) {
        inside = sortableFloat(order.depth);
    } else if (mode == Renderer::SortMode::Y) {
        inside = sortableFloat(standingY + order.sortOffset);
    }
    return (static_cast<std::uint64_t>(layer) << 32U) | inside;
}

} // namespace haylen::graphics2d
