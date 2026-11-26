#pragma once

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>

#include <atomic>
#include <cstdint>
#include <memory>

#include "platform/FrameCallbackGate.hpp"

struct wl_callback;
struct wl_interface;
struct wl_proxy;

namespace haylen::platform::sdl {

// Paces the frames of a window of a Wayland session by the frame callbacks of its surface, the way the compositor tells a client that it would show a new frame, so the swapchain never waits for a compositor that holds its callbacks, such as one whose outputs sleep. A frame presents once the callback of the last frame that presented arrived, and the callbacks wake the main callbacks of SDL, which wait for events. Once the compositor holds the callbacks, a timer at the hidden rate wakes them too and plays frames that present nothing, so timers, the network and the audio of the app go on, as `FrameCallbackGate` decides. A window that the compositor suspends, which SDL reports as occluded, presents nothing until it shows again. The callbacks come from the library of Wayland that SDL loaded, through the surface that SDL made, on the queue that SDL dispatches.
class WaylandFramePacer final {
  public:
    // Starts pacing the window, whose frames wake the main callbacks with events of the type, or returns nothing outside a Wayland session.
    [[nodiscard]] static std::unique_ptr<WaylandFramePacer> start(SDL_Window* window, std::uint32_t wakeType, int hiddenRate);

    WaylandFramePacer(const WaylandFramePacer&) = delete;
    WaylandFramePacer& operator=(const WaylandFramePacer&) = delete;
    ~WaylandFramePacer();

    // Ticks the timer at the frames per second, which it keeps while the rate stays the same.
    void setHiddenRate(int framesPerSecond);
    void setSuspended(bool value) noexcept;

    // Handles a wake event, which a tick of the timer sends with its own code.
    void receiveWake(const SDL_UserEvent& event) noexcept;

    // Whether a frame came since the runtime took the last one, which it takes.
    [[nodiscard]] bool takeFrame() noexcept;

    // Whether the frame that plays now may present, which asks the compositor for its callback.
    [[nodiscard]] bool beginFrame();

    // Ends a frame that may present, and commits the surface for its callback when it drew nothing, so the compositor still answers when it would show a frame.
    void endFrame(bool presented);

    // Keeps the callbacks coming through a refresh that the frame rate of the app skips, with a commit that holds no new frame.
    void skipFrame();

  private:
    // The functions of the library of Wayland that SDL loaded, which the pacer finds without loading it again.
    struct Library {
        wl_proxy* (*marshal)(wl_proxy*, std::uint32_t, const wl_interface*, std::uint32_t, std::uint32_t, ...) = nullptr;
        int (*addListener)(wl_proxy*, void (**)(), void*) = nullptr;
        void (*destroy)(wl_proxy*) = nullptr;
        std::uint32_t (*getVersion)(wl_proxy*) = nullptr;
        const wl_interface* callbackInterface = nullptr;
    };

    static constexpr std::int32_t kTickCode = 1;

    WaylandFramePacer(const Library& functions, wl_proxy* windowSurface, std::uint32_t type);

    static void receiveCallback(void* data, wl_callback* callback, std::uint32_t time);
    [[nodiscard]] static double getSeconds() noexcept;
    static std::uint64_t tickTimer(void* data, SDL_TimerID timer, std::uint64_t interval);
    void push(std::int32_t code) noexcept;

    Library library;
    wl_proxy* surface = nullptr;
    std::uint32_t wakeType = 0;
    FrameCallbackGate gate;
    wl_proxy* pending = nullptr;
    bool frameDue = false;
    SDL_TimerID timer = 0;
    int timerRate = 0;
    // Whether a tick waits in the queue of SDL, so the timer thread adds no other.
    std::atomic<bool> tickPending = false;
};

} // namespace haylen::platform::sdl
