#include "platform/NativeViews.hpp"

#include <algorithm>

#include "haylen/core/Log.hpp"

namespace haylen::platform {

void NativeViews::reserveInsets(std::string_view key, const math::Insets& insets) {
    const math::Insets reserved{.left = std::max(0.0F, insets.left), .top = std::max(0.0F, insets.top), .right = std::max(0.0F, insets.right), .bottom = std::max(0.0F, insets.bottom)};
    const std::scoped_lock lock(mutex);
    reservations.insert_or_assign(std::string(key), reserved);
}

void NativeViews::releaseInsets(std::string_view key) {
    const std::scoped_lock lock(mutex);
    if (const auto found = reservations.find(key); found != reservations.end()) {
        reservations.erase(found);
    }
}

math::Insets NativeViews::getReservedInsets() const {
    const std::scoped_lock lock(mutex);
    math::Insets largest;
    for (const auto& [key, insets] : reservations) {
        largest = {.left = std::max(largest.left, insets.left), .top = std::max(largest.top, insets.top), .right = std::max(largest.right, insets.right), .bottom = std::max(largest.bottom, insets.bottom)};
    }
    return largest;
}

void NativeViews::coverApp() {
    const std::scoped_lock lock(mutex);
    ++covers;
}

void NativeViews::uncoverApp() {
    {
        const std::scoped_lock lock(mutex);
        if (covers > 0) {
            --covers;
            return;
        }
    }
    core::Log::error("A native view uncovered the app without covering it first.");
}

bool NativeViews::isAppCovered() const {
    const std::scoped_lock lock(mutex);
    return covers > 0;
}

} // namespace haylen::platform
