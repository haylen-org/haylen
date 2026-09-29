#pragma once

#include <miniaudio.h>

#include <mutex>
#include <vector>

#include "haylen/audio/Mixer.hpp"
#include "haylen/audio/Session.hpp"

namespace haylen::audio {

// The audio context and playback device of a mixer, or no device at all for a mixer that renders on demand. The output runs while it is neither suspended nor interrupted. The device plays with the game usage of AAudio on Android, the session category of the setup on iOS and tvOS and the AudioWorklet output of the page in browsers, and its notifications wait in a queue that the frame thread takes.
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

    // Opens the context and the device again, which sets the iOS audio session category and activates the session again, and restarts the output unless it is suspended. Throws std::runtime_error when the system refuses, and the output stays interrupted.
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

    // Queues an event from any thread.
    void report(Mixer::DeviceEvent event);
    [[nodiscard]] std::vector<Mixer::DeviceEvent> takeEvents();

  private:
    static void onData(ma_device* playback, void* output, const void* input, ma_uint32 frames);
    static void onNotification(const ma_device_notification* notification) noexcept;
    [[nodiscard]] static ma_ios_session_category toSessionCategory(Session::Category category) noexcept;

    void open();
    void close() noexcept;
    void apply();

    Mixer::Setup setup;
    ma_context context{};
    ma_device device{};
    ma_engine* engine = nullptr;
    bool opened = false;
    bool suspended = false;
    bool interrupted = false;
    bool running = false;
    std::mutex eventMutex;
    std::vector<Mixer::DeviceEvent> events;
};

} // namespace haylen::audio
