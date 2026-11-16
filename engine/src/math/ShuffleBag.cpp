#include "haylen/math/ShuffleBag.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

#include "haylen/math/Random.hpp"

namespace haylen::math {

ShuffleBag::ShuffleBag(std::span<const std::uint32_t> counts) {
    std::uint64_t total = 0;
    for (const std::uint32_t count : counts) {
        total += count;
    }
    if (total > kMaxItems) {
        throw std::invalid_argument("A shuffle bag holds at most " + std::to_string(kMaxItems) + " items.");
    }

    items.reserve(static_cast<std::size_t>(total));
    for (std::size_t item = 0; item < counts.size(); ++item) {
        items.insert(items.end(), counts[item], item);
    }

    if (items.empty()) {
        throw std::invalid_argument("A shuffle bag needs at least one item.");
    }

    remaining = items.size();
}

// Swaps a random undealt item to the end of the undealt range and deals it, which is one step of a Fisher-Yates shuffle.
std::size_t ShuffleBag::next(Random& random) noexcept {
    if (remaining == 0) {
        remaining = items.size();
    }

    const auto chosen = static_cast<std::size_t>(random.nextU64() % remaining);
    --remaining;
    std::swap(items[chosen], items[remaining]);
    return items[remaining];
}

void ShuffleBag::refill() noexcept {
    remaining = items.size();
}

} // namespace haylen::math
