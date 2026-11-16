#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "gpu/DeviceFactory.hpp"
#include "haylen/gpu/Device.hpp"
#include "haylen/math/Vec2.hpp"
#include "platform/DeviceRecovery.hpp"
#include "platform/FrameRateGate.hpp"
#include "platform/FrameTimer.hpp"
#include "platform/Touches.hpp"
#include "platform/worker/Events.hpp"
#include "platform/worker/Gamepads.hpp"
#include "platform/worker/Window.hpp"

namespace haylen::platform::worker {

class Host;

// Plays apps in a dedicated worker of the browser, the Worker mode of the web, which a page starts with `haylen-worker.js` so it can end the worker when the app stops answering. The runtime draws into the `OffscreenCanvas` that the page handed the worker, runs its frames on the animation frames of the worker and takes the input, the size of the canvas and the controllers that the page reads and posts. It calls the player as the window opens, draws its frames, receives its events and closes, as the platform layer of SDL does on the page.
class Runtime final {
  public:
    // The kinds of changes of the window that the page reports, in the order the page numbers them.
    enum class Change : std::uint8_t {
        FocusGained,
        FocusLost,
        MouseEnter,
        MouseLeave,
        Resized,
    };

    // Starts the frames of the command line and returns at once, while the animation frames of the worker keep running them. Returns 1 when the worker runs no animation frames or the player could not start.
    static int run(int argc, char* argv[]);

    // The runtime of the worker while its window lives.
    [[nodiscard]] static Runtime* find() noexcept {
        return running;
    }

    [[nodiscard]] Window& getWindow() noexcept {
        return window;
    }

    [[nodiscard]] Gamepads& getGamepads() noexcept {
        return gamepads;
    }

    // Hands over the GPU device that started with the window, or creates another one for the window, as an app that replaces the last one asks for it, once `prepareDevice` reports its graphics API prepared.
    [[nodiscard]] std::unique_ptr<gpu::Device> createDevice();
    [[nodiscard]] bool prepareDevice();

    // Ends the frames at the next animation frame, which closes the window and ends the player.
    void close() noexcept {
        closing = true;
    }

    // Paces the frames at the frames per second of the app, or at every animation frame for 0.
    void setFrameRate(int framesPerSecond);

    // Caps the framebuffer pixels in each CSS pixel of the canvas, or takes every pixel for 0, and resizes the drawable of the GPU device after it.
    void setMaxPixelDensity(float cap);

    void receiveKey(std::string_view code, bool down, bool repeat, int modifiers);
    void receiveMouse(Events::Mouse change, int button, math::Vec2 position, math::Vec2 movement, int modifiers);
    void receiveTouch(std::uint64_t id, Touches::Phase phase, math::Vec2 position, int modifiers);
    void receiveScroll(math::Vec2 value, bool precise, int modifiers);
    void receiveChange(Change change);

    // A hidden page stops the animation frames of its workers, so the frames run from timers while the page hides, at most at `kHiddenRate`, which keeps timers and network callbacks of the app going.
    void setPageVisible(bool visible);

  private:
    static constexpr int kHiddenRate = 60;

    // The values that the page posts for each controller: its index, its buttons and its axes.
    static constexpr std::size_t kRecordSize = 1 + Gamepads::kButtonCount + Gamepads::kAxisCount;
    static constexpr std::size_t kMaxControllers = 8;

    static inline Runtime* running = nullptr;

    static void onFrame(void* state);
    [[nodiscard]] static bool hasAnimationFrames() noexcept;
    [[nodiscard]] static gpu::DeviceFactory::Options getDeviceOptions() noexcept;

    [[nodiscard]] int initialize(int argc, char* argv[]);
    void frame();
    void start();
    [[nodiscard]] bool openWindow();
    void recover();
    void deliver(const Event& event);
    void followDrawable();
    void readGamepads();
    void updateTiming();
    void quit() noexcept;

    Window window;
    Events events;
    Gamepads gamepads;
    FrameTimer timer;
    FrameRateGate frameRate;
    std::unique_ptr<gpu::Device> openedDevice;
    std::vector<gpu::Device::Api> apis;
    std::size_t nextApi = 0;
    bool started = false;
    bool closing = false;
    bool hidden = false;
    bool recovering = false;
    DeviceRecovery deviceRecovery;

    // The controllers that the page posted last, with the revision of their ids, which the runtime reads again only when the page connected or lost one.
    std::vector<float> gamepadValues = std::vector<float>(kRecordSize * kMaxControllers);
    std::vector<std::string> gamepadIds;
    std::vector<Gamepads::Record> gamepadRecords;
    int gamepadRevision = -1;
};

} // namespace haylen::platform::worker
