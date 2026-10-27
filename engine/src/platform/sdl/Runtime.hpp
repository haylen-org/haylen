#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>

#include "gpu/DeviceFactory.hpp"
#include "haylen/core/AppConfig.hpp"
#include "haylen/gpu/Device.hpp"
#include "haylen/platform/Event.hpp"
#include "platform/sdl/Events.hpp"
#include "platform/sdl/FrameTimer.hpp"
#include "platform/sdl/Window.hpp"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif
#if !defined(__APPLE__) || TARGET_OS_OSX
#include "platform/sdl/Gamepads.hpp"
#endif
#if defined(_WIN32) || defined(__linux__)
#include "platform/sdl/TextInput.hpp"
#endif
#if defined(__APPLE__) && !TARGET_OS_OSX
#include "platform/SurfaceCheck.hpp"
#endif

namespace haylen::platform::sdl {

// Plays apps in the window that SDL opens, through the main callbacks of SDL, which run the same frames on the desktops, on the display link of UIKit and on the animation frames of the browser. It opens the window for the graphics API that starts first among the ones the app prefers, calls the player of the platform as the window opens, draws its frames, receives its events and closes, and hands the player the GPU device of that window.
class Runtime final {
  public:
    // Plays the command line until the window closes and returns the status the process exits with. Apple apps enter here through `haylen_main` after registering their native handlers, UIKit and the browser keep running the frames once it returns, and Windows and Linux processes enter through the entry point of the runtime.
    static int run(int argc, char* argv[]);

    // The runtime of the process while its window lives, which the application delegate of macOS reaches when the system asks the app to quit.
    [[nodiscard]] static Runtime* find() noexcept {
        return running;
    }

    [[nodiscard]] Window& getWindow() noexcept {
        return window;
    }

    [[nodiscard]] bool isHighDpi() const noexcept {
        return highDpi;
    }

    [[nodiscard]] bool isTransparent() const noexcept {
        return transparent;
    }

#if !defined(__APPLE__) || TARGET_OS_OSX
    [[nodiscard]] Gamepads& getGamepads() noexcept {
        return gamepads;
    }
#endif

#if defined(_WIN32) || defined(__linux__)
    [[nodiscard]] TextInput& getTextInput() noexcept {
        return textInput;
    }
#endif

    // Hands over the GPU device that started with the window, or creates another one for the window, as an app that replaces the last one asks for it.
    [[nodiscard]] std::unique_ptr<gpu::Device> createDevice();

    // Ends the frames at the next iteration, which closes the window and ends the process of desktop apps.
    void close() noexcept {
        closing = true;
    }

    // Queues an event that the platform read on its own, in the order of the events of SDL, such as the scrolls of macOS trackpads with their phases.
    void post(const platform::Event& event);

    // Queues a scroll of the wheels and trackpads that the web page read over the canvas, in steps of a wheel or in points of the page for a precise device, with the bits of shift, control, alt and the super key held.
    static void scroll(math::Vec2 value, bool precise, int modifiers);

    // Ends the app at once as its window closes, after telling it that the system asked it to quit, for the system that ends the process right after.
    void terminate();

  private:
    // The highest number of iterations per second while the window shows nothing, minimized or hidden, which the display no longer paces, so the app keeps running timers and network callbacks without spending a core.
    static constexpr const char* kHiddenRate = "60";

#if !defined(__APPLE__) || TARGET_OS_OSX
    static constexpr SDL_InitFlags kSubsystems = SDL_INIT_VIDEO | SDL_INIT_GAMEPAD;
#else
    // UIKit apps read their controllers through GameController, as `platform::AppleGamepads` does, so SDL starts no gamepads there.
    static constexpr SDL_InitFlags kSubsystems = SDL_INIT_VIDEO;
#endif

    static inline Runtime* running = nullptr;

    static int enter(int argc, char* argv[]);
    static SDL_AppResult onInitialize(void** state, int argc, char* argv[]);
    static SDL_AppResult onIterate(void* state);
    static bool SDLCALL onFilter(void* state, SDL_Event* event);
    static SDL_AppResult onEvent(void* state, SDL_Event* event);
    static void onQuit(void* state, SDL_AppResult result);
    static void describeApp(const core::AppConfig& config);
    [[nodiscard]] static gpu::DeviceFactory::Options getDeviceOptions() noexcept;

    SDL_AppResult initialize(int argc, char* argv[]);
    [[nodiscard]] SDL_AppResult start();
    [[nodiscard]] bool openWindow();
    [[nodiscard]] SDL_AppResult iterate();
    [[nodiscard]] bool recover();
    [[nodiscard]] SDL_AppResult handle(const SDL_Event& event);
    void deliver(const SDL_Event& event);
    void quit() noexcept;
    [[nodiscard]] bool takes(const SDL_Event& event) const noexcept;
    [[nodiscard]] Events::Context getContext() const noexcept;
    void followWindow(const SDL_Event& event);

    Window window;
    Events events;
#if !defined(__APPLE__) || TARGET_OS_OSX
    Gamepads gamepads;
#endif
#if defined(_WIN32) || defined(__linux__)
    TextInput textInput{window};
#endif
    FrameTimer timer;
    std::vector<platform::Event> translated;
    std::unique_ptr<gpu::Device> openedDevice;

    // The graphics APIs to try in order, and the next one to try.
    std::vector<gpu::Device::Api> apis;
    std::size_t nextApi = 0;
    bool highDpi = true;
    bool transparent = false;
    bool started = false;
    bool closing = false;

    // Whether the device of the app stopped working, until its replacement exists.
    bool recovering = false;

    // Whether the frames run at the rate of a window that shows nothing.
    bool paced = false;
#if defined(__APPLE__) && !TARGET_OS_OSX

    // The check of debug builds that the view and the drawable of the window cover the window at every frame.
    SurfaceCheck surfaceCheck;
#endif

    // The presses and releases of the back button that the player took as they arrived, which reach no app.
    int takenBacks = 0;

    // The events that the platform read on its own, which wait for the event of SDL that keeps their place in the queue.
    std::uint32_t postedType = 0;
    std::vector<platform::Event> posted;
    std::size_t postedRead = 0;
};

} // namespace haylen::platform::sdl
