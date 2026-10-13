#include "platform/AccessibilityRelay.hpp"

namespace haylen::platform {

std::mutex& AccessibilityRelay::mutex = *new std::mutex();
Accessibility* AccessibilityRelay::accessibility = nullptr;

void AccessibilityRelay::attach(Accessibility& value) noexcept {
    const std::scoped_lock lock(mutex);
    accessibility = &value;
}

void AccessibilityRelay::detach(const Accessibility& value) noexcept {
    const std::scoped_lock lock(mutex);
    if (accessibility == &value) {
        accessibility = nullptr;
    }
}

void AccessibilityRelay::perform(const Accessibility::Request& request) {
    const std::scoped_lock lock(mutex);
    if (accessibility != nullptr) {
        accessibility->perform(request);
    }
}

} // namespace haylen::platform
