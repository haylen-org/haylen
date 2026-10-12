#include "platform/GamepadSlots.hpp"

#include <algorithm>
#include <bitset>

namespace haylen::platform {

// A slot that frees up stays empty until the next update, so the app hears the controller that left before another one takes its slot.
void GamepadSlots::update(std::span<const void* const> connected) {
    std::bitset<input::Input::kMaxGamepads> freed;
    for (std::size_t slot = 0; slot < controllers.size(); ++slot) {
        if (controllers[slot] != nullptr && std::ranges::find(connected, controllers[slot]) == connected.end()) {
            controllers[slot] = nullptr;
            freed.set(slot);
        }
    }

    for (const void* controller : connected) {
        if (std::ranges::find(controllers, controller) != controllers.end()) {
            continue;
        }
        std::size_t slot = 0;
        while (slot < controllers.size() && (controllers[slot] != nullptr || freed.test(slot))) {
            ++slot;
        }
        if (slot == controllers.size()) {
            return;
        }
        controllers[slot] = controller;
    }
}

const void* GamepadSlots::getController(std::size_t slot) const noexcept {
    return slot < controllers.size() ? controllers[slot] : nullptr;
}

} // namespace haylen::platform
