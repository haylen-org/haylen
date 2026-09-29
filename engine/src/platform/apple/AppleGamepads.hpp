#pragma once

#import <GameController/GameController.h>

#include <span>

#include "haylen/input/GamepadState.hpp"
#include "platform/GamepadSlots.hpp"

namespace haylen::platform {

// Controllers read through GameController, each one in the slot it took when it connected. The Siri Remote of the Apple TV counts as a controller too.
class AppleGamepads final {
  public:
    static void poll(std::span<input::GamepadState> states);

  private:
    static GamepadSlots slots;

    [[nodiscard]] static GCController* find(NSArray<GCController*>* controllers, const void* identity);
    static void read(GCExtendedGamepad* pad, input::GamepadState& state);
};

} // namespace haylen::platform
