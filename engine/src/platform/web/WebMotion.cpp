#include "platform/web/WebMotion.hpp"

#include <emscripten/emscripten.h>

#include <array>

#include "platform/MotionFrame.hpp"

// clang-format off
EM_JS(int, haylen_js_motion_support, (), {
    return Module.haylen.motion.support();
});

EM_JS(void, haylen_js_set_motion, (int sensors), {
    Module.haylen.motion.set(sensors);
});

EM_JS(int, haylen_js_read_motion, (float* values), {
    return Module.haylen.motion.read(values);
});
// clang-format on

namespace haylen::platform {

// The page tells which events the browser has, which say nothing of the hardware until they fire, and fires none outside a secure context.
input::MotionSensors::Support WebMotion::getSupport() {
    const int support = haylen_js_motion_support();
    input::MotionSensors::Support result{.sensors = input::MotionSensors::Sensors(static_cast<unsigned long>(support & 7))};
    if ((support & 8) != 0) {
        result.reason = "Browsers give the motion sensors only to pages served over https or from localhost.";
    } else if (result.sensors.none()) {
        result.reason = "This browser has no motion events.";
    }
    return result;
}

void WebMotion::set(input::MotionSensors::Sensors sensors) {
    haylen_js_set_motion(static_cast<int>(sensors.to_ulong()));
}

void WebMotion::read(input::MotionState& state) {
    std::array<float, 9> values{};
    const int read = haylen_js_read_motion(values.data());
    MotionFrame::readBrowser(state, values, input::MotionSensors::Sensors(static_cast<unsigned long>(read & 7)), read >> 8);
}

} // namespace haylen::platform
