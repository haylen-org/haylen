#pragma once

#import <GameController/GameController.h>

#include <span>
#include <vector>

#include "haylen/input/GamepadState.hpp"
#include "platform/GamepadSlots.hpp"

namespace haylen::platform {

// Controllers read through GameController, each one in the slot it took when it connected. The Siri Remote of the Apple TV counts as a controller too.
class AppleGamepads final {
  public:
    static void poll(std::span<input::GamepadState> states);

    // Plays a continuous haptic event on the controller in the slot through the haptics engine of the controller, whose intensity follows the stronger motor and whose sharpness follows the share of the high frequency motor, or stops it.
    static void rumble(std::size_t slot, float low, float high, float seconds);

  private:
    static GamepadSlots slots;
    static std::vector<const void*> connected;

    [[nodiscard]] static GCController* find(NSArray<GCController*>* controllers, const void* identity);
    static void read(GCExtendedGamepad* pad, input::GamepadState& state);
    [[nodiscard]] static input::GamepadLabels getLabels(GCExtendedGamepad* pad);

    // The keys of the objects that a controller keeps for its rumble: its haptics engine and the player of the last rumble.
    static char engineKey;
    static char playerKey;
};

} // namespace haylen::platform
