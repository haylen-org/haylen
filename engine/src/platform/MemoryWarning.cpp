#include "platform/MemoryWarning.hpp"

namespace haylen::platform {

std::atomic<std::uint8_t> MemoryWarning::raised = kNone;

// A warning never lowers a critical level that waits for the frame.
void MemoryWarning::raise(Event::MemoryLevel level) noexcept {
    const auto value = static_cast<std::uint8_t>(static_cast<std::uint8_t>(level) + 1);
    std::uint8_t current = raised.load(std::memory_order_relaxed);
    while (current < value && !raised.compare_exchange_weak(current, value, std::memory_order_acq_rel)) {}
}

std::optional<Event::MemoryLevel> MemoryWarning::take() noexcept {
    const std::uint8_t value = raised.exchange(kNone, std::memory_order_acq_rel);
    if (value == kNone) {
        return std::nullopt;
    }
    return static_cast<Event::MemoryLevel>(value - 1);
}

} // namespace haylen::platform
