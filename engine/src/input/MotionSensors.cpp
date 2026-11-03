#include "haylen/input/MotionSensors.hpp"

#include <algorithm>
#include <stdexcept>

#include "haylen/input/Input.hpp"
#include "platform/Host.hpp"

namespace haylen::input {

MotionSensors::MotionSensors(platform::Host& owner) : host(owner) {}

MotionSensors::~MotionSensors() {
    clear();
}

void MotionSensors::clear() {
    requests.clear();
    settled.clear();
    if (deviceSensors.any()) {
        deviceSensors.reset();
        deviceRate = 0.0F;
        host.setMotion({}, 0.0F);
    }
    if (gamepadSensors.any()) {
        gamepadSensors.reset();
        host.setGamepadMotion({});
    }
}

std::string MotionSensors::describe(const Sensors& sensors) {
    std::string names;
    std::size_t written = 0;
    for (std::size_t index = 0; index < sensors.size(); ++index) {
        if (!sensors.test(index)) {
            continue;
        }
        ++written;
        if (written > 1) {
            names += written == sensors.count() ? " and " : ", ";
        }
        names += "\"" + std::string(Controls::motionSensorName(static_cast<MotionSensor>(index))) + "\"";
    }
    return names;
}

std::uint64_t MotionSensors::start(const Options& options, Callback callback) {
    if (options.sensors.none()) {
        throw std::invalid_argument("The motion sensors need at least one sensor, such as \"accelerometer\".");
    }
    if (!options.device && !options.gamepads) {
        throw std::invalid_argument("The motion sensors need the device, the gamepads or both.");
    }
    if (!(options.rate >= kMinRate && options.rate <= kMaxRate)) {
        throw std::invalid_argument("The rate of the motion sensors must be from 1 to 200 readings per second.");
    }

    const std::uint64_t id = nextId++;
    if (!options.device) {
        requests.push_back({.id = id, .options = options, .running = true});
        settled.push_back({.callback = std::move(callback)});
        return id;
    }

    const Support support = host.getMotionSupport();
    const Sensors absent = options.sensors & ~support.sensors;
    if (absent.any()) {
        std::string message = support.sensors.none() && !support.reason.empty() ? support.reason : (absent.count() == 1 ? "The device has no motion sensor " : "The device has no motion sensors ") + describe(absent) + ".";
        settled.push_back({.callback = std::move(callback), .failure = Failure{.code = "unsupported", .message = std::move(message)}});
        return id;
    }

    requests.push_back({.id = id, .options = options, .callback = std::move(callback)});
    // clang-format off
    host.requestPermission(platform::Permission::Kind::Motion, [shared = std::weak_ptr<Inbox>(inbox), id](platform::Permission::Answer answer) {
        if (const std::shared_ptr<Inbox> alive = shared.lock()) {
            const std::lock_guard lock(alive->mutex);
            alive->answers.emplace_back(id, std::move(answer));
        }
    });
    // clang-format on
    return id;
}

void MotionSensors::stop(std::uint64_t id) {
    const auto found = std::ranges::find(requests, id, &Request::id);
    if (found == requests.end()) {
        return;
    }
    if (!found->running) {
        settled.push_back({.callback = std::move(found->callback), .failure = Failure{.code = "cancelled", .message = "The app stopped the motion sensors before they started."}});
    }
    requests.erase(found);
}

bool MotionSensors::isRunning(std::uint64_t id) const noexcept {
    const auto found = std::ranges::find(requests, id, &Request::id);
    return found != requests.end() && found->running;
}

bool MotionSensors::isWaiting(std::uint64_t id) const noexcept {
    const auto found = std::ranges::find(requests, id, &Request::id);
    return found != requests.end() && !found->running;
}

MotionSensors::Request* MotionSensors::find(std::uint64_t id) noexcept {
    const auto found = std::ranges::find(requests, id, &Request::id);
    return found != requests.end() ? &*found : nullptr;
}

// A state that the person did not settle, such as a browser that asks only during a tap, fails the request with the reason, and the sensors run where the platform tells nothing before they start.
std::optional<MotionSensors::Failure> MotionSensors::readAnswer(const platform::Permission::Answer& answer) {
    switch (answer.state) {
    case platform::Permission::State::Unsupported:
        return Failure{.code = "unsupported", .message = answer.message, .missing = answer.missing};
    case platform::Permission::State::Denied:
        return Failure{.code = "denied", .message = platform::Permission::refusal(platform::Permission::Kind::Motion)};
    case platform::Permission::State::Prompt:
        return Failure{.code = "failed", .message = answer.message.empty() ? "The platform did not ask the person for the motion sensors." : answer.message};
    case platform::Permission::State::Granted:
    case platform::Permission::State::Unknown:
        break;
    }
    return std::nullopt;
}

void MotionSensors::settle(std::uint64_t id, std::optional<Failure> failure) {
    Request* request = find(id);
    if (request == nullptr) {
        return;
    }
    settled.push_back({.callback = std::move(request->callback), .failure = std::move(failure)});
    if (settled.back().failure) {
        std::erase_if(requests, [id](const Request& candidate) { return candidate.id == id; });
        return;
    }
    request->running = true;
}

void MotionSensors::pump() {
    {
        const std::lock_guard lock(inbox->mutex);
        answers.swap(inbox->answers);
    }
    for (const auto& [id, answer] : answers) {
        settle(id, readAnswer(answer));
    }
    answers.clear();

    // A callback may start or stop requests, which settle into the list again for the next pump.
    delivering.swap(settled);
    for (Settled& entry : delivering) {
        entry.callback(std::move(entry.failure));
    }
    delivering.clear();
}

void MotionSensors::update(bool foreground, Input& input) {
    Sensors device;
    Sensors gamepads;
    float rate = 0.0F;
    for (const Request& request : requests) {
        if (!request.running || !foreground) {
            continue;
        }
        if (request.options.device) {
            device |= request.options.sensors;
            rate = std::max(rate, request.options.rate);
        }
        if (request.options.gamepads) {
            gamepads |= request.options.sensors;
        }
    }

    if (device != deviceSensors || rate != deviceRate) {
        deviceSensors = device;
        deviceRate = rate;
        host.setMotion(device, rate);
    }
    if (gamepads != gamepadSensors) {
        gamepadSensors = gamepads;
        host.setGamepadMotion(gamepads);
    }

    // A sensor that does not run reads zero, whatever the platform left in its place.
    readings = {};
    if (deviceSensors.any()) {
        host.readMotion(readings);
        readings.sensors &= deviceSensors;
    }
    if (!readings.sensors.test(static_cast<std::size_t>(MotionSensor::Accelerometer))) {
        readings.acceleration = {};
    }
    if (!readings.sensors.test(static_cast<std::size_t>(MotionSensor::Gyroscope))) {
        readings.rotationRate = {};
    }
    if (!readings.sensors.test(static_cast<std::size_t>(MotionSensor::Attitude))) {
        readings.attitude = MotionState{}.attitude;
    }
    input.updateMotion(readings);
}

} // namespace haylen::input
