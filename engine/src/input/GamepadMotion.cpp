#include "haylen/input/GamepadMotion.hpp"

#include <algorithm>
#include <stdexcept>

#include "platform/Host.hpp"

namespace haylen::input {

GamepadMotion::GamepadMotion(platform::Host& owner) : host(owner) {}

GamepadMotion::~GamepadMotion() {
    clear();
}

void GamepadMotion::clear() {
    requests.clear();
    if (running.any()) {
        running.reset();
        host.setGamepadMotion({});
    }
}

std::uint64_t GamepadMotion::start(const Sensors& sensors) {
    if (sensors.none()) {
        throw std::invalid_argument("The motion of gamepads needs at least one sensor, such as \"accelerometer\".");
    }

    const std::uint64_t id = nextId++;
    requests.push_back({.id = id, .sensors = sensors});
    return id;
}

void GamepadMotion::stop(std::uint64_t id) {
    std::erase_if(requests, [id](const Request& request) { return request.id == id; });
}

bool GamepadMotion::isRunning(std::uint64_t id) const noexcept {
    return std::ranges::find(requests, id, &Request::id) != requests.end();
}

void GamepadMotion::update(bool foreground) {
    Sensors wanted;
    if (foreground) {
        for (const Request& request : requests) {
            wanted |= request.sensors;
        }
    }

    if (wanted != running) {
        running = wanted;
        host.setGamepadMotion(wanted);
    }
}

} // namespace haylen::input
