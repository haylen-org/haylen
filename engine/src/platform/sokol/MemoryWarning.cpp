#include "platform/sokol/MemoryWarning.hpp"

namespace haylen::platform {

std::atomic<bool> MemoryWarning::raised = false;

void MemoryWarning::raise() noexcept {
    raised.store(true, std::memory_order_release);
}

bool MemoryWarning::take() noexcept {
    return raised.exchange(false, std::memory_order_acq_rel);
}

} // namespace haylen::platform
