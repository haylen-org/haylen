#pragma once

#import <GameController/GameController.h>

#include "haylen/input/GamepadState.hpp"

namespace haylen::platform {

// The Siri Remote of the Apple TV read as a gamepad. A swipe on its touch surface moves the left stick and, past half its travel, presses the directional pad, a click of the surface presses south and play and pause presses west. Its Menu button stays the back button of the platform.
class AppleRemote final {
  public:
    static void read(GCMicroGamepad* remote, input::GamepadState& state);

  private:
    static constexpr float kPressThreshold = 0.5F;
};

} // namespace haylen::platform
