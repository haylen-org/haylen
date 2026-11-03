#pragma once

#include <cstdint>
#include <functional>
#include <unordered_map>

#include "haylen/input/MotionSensors.hpp"
#include "haylen/input/MotionState.hpp"
#include "haylen/platform/Permission.hpp"

namespace haylen::platform {

// The motion of the device that `platform/web/haylen-runtime.js` reads from the `devicemotion` and `deviceorientation` events of the page, which it listens to only while sensors run. Browsers fire them only in a secure context, and Safari asks the person first, which it does only during a tap or a click.
class WebMotion final {
  public:
    [[nodiscard]] static input::MotionSensors::Support getSupport();

    // Asks the browser for the motion sensors where it gates them, and the page answers through `answer` with the same id.
    static void authorize(std::function<void(Permission::Answer answer)> callback);
    static void answer(double id, int result);

    static void set(input::MotionSensors::Sensors sensors);
    static void read(input::MotionState& state);

    // The state of the permission of the motion sensors, which a `Permission::Kind` of the motion sensors answers with: `prompt` in Safari until the page asked in this visit, and `granted` in the browsers that do not ask.
    [[nodiscard]] static Permission::State getPermission();

  private:
    // The answers of the page: allowed, refused, asked outside a tap or a click, or failed.
    enum class Answer : std::uint8_t {
        Granted,
        Denied,
        NoGesture,
        Failed,
    };

    static std::unordered_map<std::uint64_t, std::function<void(Permission::Answer)>>& requests;
    static std::uint64_t nextRequest;
};

} // namespace haylen::platform
