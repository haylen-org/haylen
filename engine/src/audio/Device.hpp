#pragma once

#include <miniaudio.h>

#include <mutex>
#include <optional>
#include <string_view>
#include <vector>

#include "haylen/audio/Mixer.hpp"
#include "haylen/audio/Session.hpp"

namespace haylen::audio {

// The audio context and playback device of a mixer, or no device at all for a mixer that renders on demand. The output runs while it is neither suspended nor interrupted. The device plays with the game usage of AAudio on Android, the session category of the setup on iOS and tvOS and the backend the host gives it, such as the `AudioWorklet` output of the page in browsers, and its notifications wait in a queue that the frame thread takes. A device that the system refuses leaves the output unavailable, and the log warns once each time the output becomes unavailable.
class Device final {
  public:
    explicit Device(const Mixer::Setup& value);
    ~Device();

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    // Returns the device the engine mixes into, or null without one.
    [[nodiscard]] ma_device* get() noexcept {
        return opened ? &device : nullptr;
    }

    // Starts the output, which reads the engine from then on.
    void start(ma_engine& source);

    void suspend();
    void resume();
    void interrupt();

    // Opens the context and the device again after an interruption, or while the output is unavailable, which sets the iOS audio session category and activates the session again, and restarts the output unless it is suspended.
    void endInterruption();

    [[nodiscard]] bool hasDevice() const noexcept {
        return setup.device;
    }
    [[nodiscard]] bool isRunning() const noexcept {
        return running;
    }
    [[nodiscard]] bool isInterrupted() const noexcept {
        return interrupted;
    }

    // Tells whether an open device plays the output, or plays it again once the output runs.
    [[nodiscard]] bool isAvailable() const noexcept {
        return opened && (started || !running);
    }

    // Tells whether the output runs while no device plays it, which leaves the mixer to mix the time that passes.
    [[nodiscard]] bool isSilent() const noexcept {
        return setup.device && running && !started;
    }

    // Queues an event from any thread.
    void report(Mixer::DeviceEvent event);
    [[nodiscard]] std::vector<Mixer::DeviceEvent> takeEvents();

  private:
    static void onData(ma_device* playback, void* output, const void* input, ma_uint32 frames);
    static void onNotification(const ma_device_notification* notification) noexcept;
    [[nodiscard]] static ma_ios_session_category toSessionCategory(Session::Category category) noexcept;

    // Opens the context and the device, or returns why the system refused them.
    [[nodiscard]] std::optional<std::string_view> open();

    // Opens the device and writes the reason to the log when the system refuses it and the warning is due.
    void connect(bool warn);
    void close() noexcept;
    void apply();

    Mixer::Setup setup;
    ma_context context{};
    ma_device device{};
    ma_engine* engine = nullptr;
    bool opened = false;
    bool started = false;
    bool suspended = false;
    bool interrupted = false;
    bool running = false;
    std::mutex eventMutex;
    std::vector<Mixer::DeviceEvent> events;
};

} // namespace haylen::audio
