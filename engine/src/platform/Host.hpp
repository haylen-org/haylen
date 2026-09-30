#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "graphics/DeviceSetup.hpp"
#include "graphics/FrameTarget.hpp"
#include "haylen/audio/Mixer.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/input/GamepadState.hpp"
#include "haylen/math/Insets.hpp"
#include "haylen/platform/Battery.hpp"
#include "haylen/platform/DialogRequest.hpp"
#include "haylen/platform/SystemInfo.hpp"
#include "haylen/platform/Theme.hpp"
#include "haylen/platform/Window.hpp"

namespace haylen::platform {

// Boundary between the portable engine and a platform. The Sokol runtime implements it for real devices and the headless host implements it for tests.
class Host : public Window {
  public:
    [[nodiscard]] virtual std::string_view getPlatformName() const noexcept = 0;
    [[nodiscard]] virtual graphics::DeviceSetup getGraphicsSetup() = 0;
    [[nodiscard]] virtual audio::Mixer::Setup getAudioSetup() const = 0;
    [[nodiscard]] virtual graphics::FrameTarget getFrameTarget() = 0;
    [[nodiscard]] virtual std::filesystem::path getUserDataDirectory(std::string_view identifier) = 0;
    virtual void persistUserData() = 0;
    [[nodiscard]] virtual math::Insets getSafeAreaInsets() const = 0;
    virtual void pollGamepads(std::span<input::GamepadState> gamepads) = 0;
    // Hands a platform call to the handlers of the platform, with the byte buffers that its parameters refer to, which stay valid until it returns.
    virtual void dispatchPlatformCall(std::uint64_t id, std::string_view method, std::string_view paramsJson, std::span<const std::vector<std::byte>> buffers) = 0;
    virtual void cancelPlatformCall(std::uint64_t id) = 0;

    // The screen edges that native views of plugins reserve, the largest reservation on each edge in framebuffer pixels, which the engine adds to the safe area.
    [[nodiscard]] virtual math::Insets getReservedInsets() const = 0;

    // Whether native UI of plugins covers the app, such as a full screen ad or a sign-in form.
    [[nodiscard]] virtual bool isAppCovered() const = 0;

    // The ids of the plugins whose native part the platform loaded.
    [[nodiscard]] virtual std::vector<std::string> getNativePlugins() const = 0;

    // Hands the error that stopped the app to native code as the JSON report of lua::Error::toJson, with its message, file, line, traceback and frames.
    virtual void reportError(const core::Json& report) = 0;

    // What the device and its operating system are, read once when an app starts. The graphics device names the GPU, so gpuName stays empty here.
    [[nodiscard]] virtual SystemInfo getSystemInfo() const = 0;

    // The theme and the battery as the platform last reported them, which the engine reads once per frame.
    [[nodiscard]] virtual Theme getTheme() const = 0;
    [[nodiscard]] virtual Battery getBattery() const = 0;

    // Opens the url with the app the system picks for it. The callback runs exactly once, on any thread and possibly after the engine that asked is gone, with whether an app took the url.
    virtual void openUrl(std::string_view url, std::function<void(bool opened)> callback) = 0;

    // Vibrates the device for the given seconds where it can vibrate, and does nothing elsewhere.
    virtual void vibrate(float seconds) = 0;

    // Shows a native dialog, which the platform answers exactly once through DialogRelay::resolve with the same id, from any thread. Picked files that have no path of their own are copied into folder, which the platform creates when it needs it.
    virtual void showDialog(std::uint64_t id, const DialogRequest& request, const std::filesystem::path& folder) = 0;

    // Closes a dialog that the app gave up, where the platform can. Its answer is dropped either way.
    virtual void cancelDialog(std::uint64_t id) = 0;
};

} // namespace haylen::platform
