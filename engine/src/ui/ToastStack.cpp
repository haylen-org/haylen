#include "haylen/ui/ToastStack.hpp"

#include <algorithm>
#include <utility>

namespace haylen::ui {

void ToastStack::beginFrame() noexcept {
    std::swap(previous, current);
    current.clear();
}

bool ToastStack::isBefore(const Entry& entry, std::uint64_t key, double since) noexcept {
    return entry.since < since || (entry.since == since && entry.key < key);
}

// The notices before this one take their places first: those of the last frame, and those that asked to show since then and already placed themselves this frame. Only the first ones up to the limit show, so a notice behind them waits, and a limit of zero shows every notice.
std::optional<float> ToastStack::place(std::uint64_t key, Position position, float extent, double since, std::size_t limit) {
    std::size_t ahead = 0;
    float offset = 0.0F;
    bool waits = false;
    // clang-format off
    const auto count = [&](const Entry& entry) {
        if (entry.position != position || entry.key == key || !isBefore(entry, key, since)) {
            return;
        }
        ++ahead;
        waits = waits || (limit > 0 && ahead >= limit);
        offset += entry.extent;
    };
    // clang-format on
    for (const Entry& entry : previous) {
        count(entry);
    }
    for (const Entry& entry : current) {
        if (std::ranges::none_of(previous, [&entry](const Entry& other) { return other.key == entry.key; })) {
            count(entry);
        }
    }
    current.push_back({.key = key, .position = position, .extent = extent, .since = since});
    if (waits) {
        return std::nullopt;
    }
    return offset;
}

} // namespace haylen::ui
