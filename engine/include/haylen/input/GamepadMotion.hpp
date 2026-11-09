#pragma once

#include <bitset>
#include <cstdint>
#include <vector>

#include "haylen/input/Controls.hpp"

namespace haylen::platform {
class Host;
}

namespace haylen::input {

// The requests of the app for the motion sensors of gamepads. The sensors stay off until a request wants them, run what every running request wants on every gamepad that has them, also on those that connect later, and stop when the last request ends and while the app is in the background, so they spend no battery the app does not use. Gamepads need no permission, so a request runs at once.
class GamepadMotion final {
  public:
    using Sensors = std::bitset<Controls::kMotionSensorCount>;

    explicit GamepadMotion(platform::Host& host);
    ~GamepadMotion();

    GamepadMotion(const GamepadMotion&) = delete;
    GamepadMotion& operator=(const GamepadMotion&) = delete;

    // Starts a request for the sensors and returns its id. Throws `std::invalid_argument` for a request without sensors.
    std::uint64_t start(const Sensors& sensors);

    void stop(std::uint64_t id);

    [[nodiscard]] bool isRunning(std::uint64_t id) const noexcept;

    // Ends every request and stops the sensors, which the engine does before the Lua state of the app closes.
    void clear();

    // Runs what the requests want while the app is in the foreground, and nothing in the background. The engine calls it once per frame before it reads the gamepads.
    void update(bool foreground);

  private:
    struct Request {
        std::uint64_t id = 0;
        Sensors sensors;
    };

    platform::Host& host;
    std::vector<Request> requests;
    std::uint64_t nextId = 1;
    Sensors running;
};

} // namespace haylen::input
