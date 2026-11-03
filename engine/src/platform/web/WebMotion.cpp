#include "platform/web/WebMotion.hpp"

#include <emscripten/emscripten.h>

#include <array>
#include <utility>

#include "platform/MotionFrame.hpp"
#include "platform/web/WebEntry.hpp"

// clang-format off
EM_JS(int, haylen_js_motion_support, (), {
    return Module.haylen.motion.support();
});

EM_JS(int, haylen_js_motion_permission, (), {
    return Module.haylen.motion.permission();
});

EM_JS(void, haylen_js_request_motion, (double id), {
    Module.haylen.motion.request(id);
});

EM_JS(void, haylen_js_set_motion, (int sensors), {
    Module.haylen.motion.set(sensors);
});

EM_JS(int, haylen_js_read_motion, (float* values), {
    return Module.haylen.motion.read(values);
});
// clang-format on

namespace haylen::platform {

std::unordered_map<std::uint64_t, std::function<void(Permission::Answer)>>& WebMotion::requests = *new std::unordered_map<std::uint64_t, std::function<void(Permission::Answer)>>();
std::uint64_t WebMotion::nextRequest = 1;

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

Permission::State WebMotion::getPermission() {
    switch (haylen_js_motion_permission()) {
    case 0:
        return Permission::State::Granted;
    case 1:
        return Permission::State::Prompt;
    default:
        return Permission::State::Denied;
    }
}

void WebMotion::authorize(std::function<void(Permission::Answer answer)> callback) {
    const std::uint64_t id = nextRequest++;
    requests.emplace(id, std::move(callback));
    haylen_js_request_motion(static_cast<double>(id));
}

void WebMotion::answer(double id, int result) {
    const auto found = requests.find(static_cast<std::uint64_t>(id));
    if (found == requests.end()) {
        return;
    }
    const std::function<void(Permission::Answer)> callback = std::move(found->second);
    requests.erase(found);

    switch (static_cast<Answer>(result)) {
    case Answer::Granted:
        callback({.state = Permission::State::Granted});
        return;
    case Answer::Denied:
        callback({.state = Permission::State::Denied, .message = "The person did not allow the motion sensors. The browser asks again once the page loads again."});
        return;
    case Answer::NoGesture:
        callback({.state = Permission::State::Prompt, .message = "The browser asks for the motion sensors only during a tap or a click, so start them from one."});
        return;
    case Answer::Failed:
        callback({.state = Permission::State::Prompt, .message = "The browser did not ask for the motion sensors."});
        return;
    }
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

extern "C" {
EMSCRIPTEN_KEEPALIVE void haylen_web_motion_answered(double id, int result) {
    haylen::platform::WebEntry::guard("haylen_web_motion_answered", [&] { haylen::platform::WebMotion::answer(id, result); });
}
}
