#pragma once

#include <bitset>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "haylen/input/Controls.hpp"
#include "haylen/input/MotionState.hpp"
#include "haylen/platform/Permission.hpp"

namespace haylen::platform {
class Host;
}

namespace haylen::input {

class Input;

// The requests of the app for the motion sensors of the device and of gamepads. The sensors stay off until a request wants them, run at the highest rate that a running request asks for, and stop when the last request ends and while the app is in the background, so they spend no battery the app does not use. A request for the sensors of the device waits until the platform allows them, since some platforms ask the person first. Every answer reaches its callback in a later `pump`, on the frame thread.
class MotionSensors final {
    struct Inbox;

  public:
    using Sensors = std::bitset<Controls::kMotionSensorCount>;

    static constexpr float kDefaultRate = 60.0F;
    static constexpr float kMinRate = 1.0F;
    static constexpr float kMaxRate = 200.0F;

    // What a request runs: the sensors, the readings per second it asks for, and whether it reads the device, the gamepads or both.
    struct Options {
        Sensors sensors;
        float rate = kDefaultRate;
        bool device = true;
        bool gamepads = false;
    };

    // The motion sensors that the device has, with the sentence that tells why it has none where the platform explains it, such as a TV, whose remote reports its motion as a gamepad.
    struct Support {
        Sensors sensors;
        std::string reason;
    };

    // Why a request did not start: the code that apps compare, `unsupported`, `denied`, `failed` or `cancelled`, the sentence that explains it and the requirements that the project of the app lacks.
    struct Failure {
        std::string code;
        std::string message;
        std::vector<platform::Permission::Requirement> missing;
    };

    using Callback = std::function<void(std::optional<Failure> failure)>;

    explicit MotionSensors(platform::Host& host);
    ~MotionSensors();

    MotionSensors(const MotionSensors&) = delete;
    MotionSensors& operator=(const MotionSensors&) = delete;

    // Starts a request and returns its id. Its callback runs once, with no failure once the sensors run. Throws `std::invalid_argument` for a request without sensors, for neither the device nor the gamepads, or for a rate outside `kMinRate` to `kMaxRate`.
    std::uint64_t start(const Options& options, Callback callback);

    // Ends a request, whose callback fails with `cancelled` while it still waits for its answer.
    void stop(std::uint64_t id);

    [[nodiscard]] bool isRunning(std::uint64_t id) const noexcept;

    // Whether a request still waits for the answer of the platform.
    [[nodiscard]] bool isWaiting(std::uint64_t id) const noexcept;

    // Ends every request without its answer and stops what they run, which the engine does before the Lua state of the app closes.
    void clear();

    // Delivers the answers that arrived since the last pump. The engine calls it once per frame with the other answers of the platform.
    void pump();

    // Runs what the running requests want while the app is in the foreground and nothing in the background, and hands the newest readings of the device to the input. The engine calls it once per frame before the actions update.
    void update(bool foreground, Input& input);

    [[nodiscard]] static std::string describe(const Sensors& sensors);

  private:
    struct Request {
        std::uint64_t id = 0;
        Options options;
        Callback callback;
        bool running = false;
    };

    // Answers of the platform wait here, shared with the callbacks it holds, so an answer that comes after the engine is gone touches nothing.
    struct Inbox {
        std::mutex mutex;
        std::vector<std::pair<std::uint64_t, platform::Permission::Answer>> answers;
    };

    struct Settled {
        Callback callback;
        std::optional<Failure> failure;
    };

    [[nodiscard]] static std::optional<Failure> readAnswer(const platform::Permission::Answer& answer);
    [[nodiscard]] Request* find(std::uint64_t id) noexcept;
    void settle(std::uint64_t id, std::optional<Failure> failure);

    platform::Host& host;
    std::vector<Request> requests;
    std::vector<Settled> settled;
    std::vector<Settled> delivering;
    std::vector<std::pair<std::uint64_t, platform::Permission::Answer>> answers;
    std::uint64_t nextId = 1;
    Sensors deviceSensors;
    float deviceRate = 0.0F;
    Sensors gamepadSensors;
    MotionState readings;
    std::shared_ptr<Inbox> inbox = std::make_shared<Inbox>();
};

} // namespace haylen::input
