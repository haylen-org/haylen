#include "platform/GamepadSlots.hpp"

#include <algorithm>

namespace haylen::platform {

void GamepadSlots::update(std::span<const void* const> connected) {
    for (const void*& controller : controllers) {
        if (controller != nullptr && std::ranges::find(connected, controller) == connected.end()) {
            controller = nullptr;
        }
    }

    for (const void* controller : connected) {
        if (std::ranges::find(controllers, controller) != controllers.end()) {
            continue;
        }
        const auto free = std::ranges::find(controllers, nullptr);
        if (free == controllers.end()) {
            return;
        }
        *free = controller;
    }
}

const void* GamepadSlots::getController(std::size_t slot) const noexcept {
    return slot < controllers.size() ? controllers[slot] : nullptr;
}

} // namespace haylen::platform
