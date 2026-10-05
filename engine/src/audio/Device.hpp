#pragma once

#include <miniaudio.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>
#include <vector>

#include "haylen/audio/Mixer.hpp"
#include "haylen/audio/Session.hpp"

namespace haylen::audio {

// The audio context and playback device of a mixer, or no device at all for a mixer that renders on demand. The output runs while it is neither suspended nor interrupted. The device plays with the game usage of AAudio on Android, the session category of the setup on iOS and tvOS and the backend the host gives it, such as the `AudioWorklet` output of the page in browsers, and its notifications wait in a queue that the frame thread takes.
// The system may take long to open a device, or never answer, such as an iOS simulator whose audio service hangs, so the device opens on a thread of its own and the frame thread takes the result in `update`, while the mixer goes on without sound. Browsers have no threads, and the output of the page opens at once. A device that the system refuses, or that does not answer within `kAnswerSeconds`, leaves the output unavailable, and the log warns once.
class Device final {
  public:
    // The seconds of frames that an opening waits for the system before the log says that the output does not answer.
    static constexpr float kAnswerSeconds = 3.0F;

    explicit Device(const Mixer::Setup& value);
    ~Device();

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    // Starts the output, which reads the engine from then on, and `stop` lets the engine go before the mixer destroys it.
    void start(ma_engine& source);
    void stop() noexcept;

    void suspend();
    void resume();
    void interrupt();

    // Opens the context and the device again after an interruption, or while the output is unavailable, which sets the iOS audio session category and activates the session again, and restarts the output unless it is suspended.
    void endInterruption();

    // Takes the device of an opening that finished, and counts the seconds an opening waits for the system. The mixer calls it once per frame.
    void update(float deltaSeconds);

    [[nodiscard]] bool hasDevice() const noexcept {
        return setup.device;
    }
    [[nodiscard]] bool isRunning() const noexcept {
        return running;
    }
    [[nodiscard]] bool isInterrupted() const noexcept {
        return interrupted;
    }

    // Tells whether the system is still opening the device.
    [[nodiscard]] bool isOpening() const noexcept {
        return opening != nullptr;
    }

    // Tells whether an open device plays the output, or plays it again once the output runs.
    [[nodiscard]] bool isAvailable() const noexcept {
        return connection != nullptr && (started || !running);
    }

    // Tells whether the output runs while no device plays it, which leaves the mixer to mix the time that passes.
    [[nodiscard]] bool isSilent() const noexcept {
        return setup.device && running && !started;
    }

    // Queues an event from any thread.
    void report(Mixer::DeviceEvent event);
    [[nodiscard]] std::vector<Mixer::DeviceEvent> takeEvents();

  private:
    // What the audio thread and the threads that open devices share with the frame thread: the engine the output reads, or null while it reads none, and the events that wait for the frame thread.
    struct Shared {
        std::atomic<ma_engine*> engine = nullptr;
        std::mutex mutex;
        std::vector<Mixer::DeviceEvent> events;
    };

    // One opening of the context and the device. The thread that opens them shares it with the device until it finished, and an opening that the device gave up closes on that thread once the system answers.
    struct Connection {
        std::shared_ptr<Shared> shared;
        ma_context context{};
        ma_device device{};
        std::mutex mutex;
        bool finished = false;
        bool abandoned = false;
        bool opened = false;
        std::optional<std::string_view> refusal;
    };

    static void onData(ma_device* playback, void* output, const void* input, ma_uint32 frames);
    static void onNotification(const ma_device_notification* notification) noexcept;
    [[nodiscard]] static ma_ios_session_category toSessionCategory(Session::Category category) noexcept;

    // Opens the context and the device of the connection, and closes them at once when the device gave the opening up meanwhile.
    static void open(Connection& target, const Mixer::Setup& setup);
    [[nodiscard]] static std::optional<std::string_view> initialize(Connection& target, const Mixer::Setup& setup);
    static void close(Connection& target) noexcept;

    // Starts an opening, whose refusal the log writes when `warn` is true.
    void connect(bool warn);
    void take();
    void disconnect() noexcept;
    void apply();

    Mixer::Setup setup;
    std::shared_ptr<Shared> shared = std::make_shared<Shared>();
    std::shared_ptr<Connection> opening;
    std::shared_ptr<Connection> connection;
    float waited = 0.0F;
    bool warnRefusal = false;
    bool warnedWaiting = false;
    bool started = false;
    bool suspended = false;
    bool interrupted = false;
    bool running = false;
};

} // namespace haylen::audio
