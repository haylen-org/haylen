#pragma once

#include "haylen/input/MotionSensors.hpp"
#include "haylen/input/MotionState.hpp"

namespace haylen::platform {

// The motion of the device that `platform/web/haylen-runtime.js` reads from the `devicemotion` and `deviceorientation` events of the page, which it listens to only while sensors run. Browsers fire them only in a secure context, and Safari asks the person first, which the permission `motion` of `Permission` does.
class WebMotion final {
  public:
    [[nodiscard]] static input::MotionSensors::Support getSupport();

    static void set(input::MotionSensors::Sensors sensors);
    static void read(input::MotionState& state);
};

} // namespace haylen::platform
