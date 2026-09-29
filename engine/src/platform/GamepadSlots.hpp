#pragma once

#include <array>
#include <cstddef>
#include <span>

#include "haylen/input/Input.hpp"

namespace haylen::platform {

// Gives every connected controller the gamepad slot it takes when it connects and keeps it there for as long as it stays connected, so a controller that disconnects never moves the others. Controllers are told apart by an identity the platform keeps for each one while it is connected.
class GamepadSlots final {
  public:
    // Frees the slots of the controllers missing from the list and gives the new ones the first free slots, in the order of the list. Controllers beyond the last slot wait until one frees up.
    void update(std::span<const void* const> connected);

    // Returns the identity of the controller in a slot, or null when the slot is free.
    [[nodiscard]] const void* getController(std::size_t slot) const noexcept;

  private:
    std::array<const void*, input::Input::kMaxGamepads> controllers{};
};

} // namespace haylen::platform
