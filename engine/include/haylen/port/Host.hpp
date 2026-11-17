#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "haylen/audio/Mixer.hpp"
#include "haylen/gpu/Device.hpp"
#include "haylen/input/GamepadMotion.hpp"
#include "haylen/input/GamepadState.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/platform/Permission.hpp"
#include "haylen/platform/TextInput.hpp"
#include "haylen/platform/Window.hpp"

namespace haylen::port {

// The window of a port as the engine drives it: its framebuffer, its scale, fullscreen, title, cursor, clipboard, text input, controllers, frame pacing, GPU device and audio setup. The runtime of a port implements it and hands it to `Player::create`, and the engine keeps everything else of the host, such as the development session, the style of the window and the services of `port::Services`. Every method runs on the frame thread.
class Host {
  public:
    Host() = default;
    Host(const Host&) = delete;
    Host& operator=(const Host&) = delete;
    virtual ~Host() = default;

    // The size of the framebuffer in pixels and the framebuffer pixels in each point of the window.
    [[nodiscard]] virtual math::Vec2 getFramebufferSize() const noexcept = 0;
    [[nodiscard]] virtual float getDpiScale() const noexcept = 0;

    // Whether the window fills its screen, and the request to fill it or leave it, which calls back exactly once with `Granted` or with the state and the sentence that say why it cannot.
    [[nodiscard]] virtual bool isFullscreen() const noexcept = 0;
    virtual void setFullscreen(bool value, std::function<void(platform::Permission::Answer answer)> callback) = 0;

    virtual void setTitle(std::string_view value) = 0;
    virtual void setCursor(platform::Window::Cursor value) = 0;
    virtual void setCursorVisible(bool value) = 0;
    virtual void setMouseLocked(bool value, std::function<void(platform::Permission::Answer answer)> callback) = 0;

    // The clipboard of the system, as `platform::Window` describes it for apps.
    virtual void setClipboard(std::string_view value, std::function<void(platform::Permission::Answer answer)> callback) = 0;
    [[nodiscard]] virtual std::string getClipboard() const = 0;
    virtual void readClipboard(std::function<void(platform::Permission::Answer answer, std::string text)> callback) = 0;

    // Whether the window opened able to compose its alpha with what lies below it.
    [[nodiscard]] virtual bool canBeTransparent() const noexcept = 0;

    // The text input of the platform, which lives as long as the process: a native field where the platform edits text with its own keyboard, or a `platform::TextInput` whose defaults type through key and character events.
    [[nodiscard]] virtual platform::TextInput& getTextInput() noexcept = 0;

    // Prepares the GPU device of the next app and returns whether `createGpuDevice` creates it now, and creates the device that draws into the window. One device exists while an app runs.
    [[nodiscard]] virtual bool prepareGpuDevice() = 0;
    [[nodiscard]] virtual std::unique_ptr<gpu::Device> createGpuDevice() = 0;

    // How the mixer of each app plays: through the audio output of `port::Services`, or without a device where the platform has no sound.
    [[nodiscard]] virtual audio::Mixer::Setup getAudioSetup() const = 0;

    // Asks the window to close as a request of the system does, which the app hears before it closes, and closes it at once without asking the app, which ends the process.
    virtual void requestClose() = 0;
    virtual void close() = 0;

    // Whether an app may end itself on the platform, which the apps of phones, TVs and web pages may not, while an app that another app launched always quits back to it.
    [[nodiscard]] virtual bool canQuit() const noexcept = 0;

    // Takes the splash screen of the platform away over the seconds, once the app drew its first complete frame.
    virtual void endSplashScreen(float fadeOutSeconds) = 0;

    // Keeps the platform responsive while the Lua debugger halts the frame thread, and returns whether the platform asks the frame to finish, such as a quit.
    [[nodiscard]] virtual bool pumpHaltedEvents() = 0;

    // The controllers of the platform, with their rumble and their motion sensors, which run only for the sensors in the set.
    virtual void pollGamepads(std::span<input::GamepadState> gamepads) = 0;
    virtual void rumbleGamepad(std::size_t index, float low, float high, float seconds) = 0;
    virtual void setGamepadMotion(input::GamepadMotion::Sensors sensors) = 0;

    virtual void setFocusable(bool value) = 0;
    virtual void setScreenSaverSuspended(bool value, std::function<void(platform::Permission::Answer answer)> callback) = 0;

    // Paces the frames at the frames per second, or at every refresh of the display for 0.
    virtual void applyFrameRate(int framesPerSecond) = 0;

    // Sizes the drawable of the window for the pixel density cap, or for every pixel of the display for 0, and the GPU device after it, as `platform::PixelDensityCap` computes it.
    virtual void applyMaxPixelDensity(float cap) = 0;
};

} // namespace haylen::port
