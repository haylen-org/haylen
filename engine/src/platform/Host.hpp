#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
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
#include "haylen/platform/AccessibilityElement.hpp"
#include "haylen/platform/AccessibilitySettings.hpp"
#include "haylen/platform/Battery.hpp"
#include "haylen/platform/DialogRequest.hpp"
#include "haylen/platform/MediaSession.hpp"
#include "haylen/platform/ScreenRequest.hpp"
#include "haylen/platform/SystemInfo.hpp"
#include "haylen/platform/Theme.hpp"
#include "haylen/platform/VideoDecoder.hpp"
#include "haylen/platform/Window.hpp"

namespace haylen::io {
class PackageReader;
}

namespace haylen::platform {

class DevelopmentSession;

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

    // The height in framebuffer pixels of the status bar that shows over the top of the app, or 0 while none shows.
    [[nodiscard]] virtual float getStatusBarHeight() const = 0;

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

    // Hands the error that stopped the app to native code as the JSON report of `lua::Error::toJson`, with its `message`, `file`, `line`, `traceback` and `frames`.
    virtual void reportError(const core::Json& report) = 0;

    // What the device and its operating system are, read once when an app starts. The graphics device names the GPU, so `gpuName` stays empty here.
    [[nodiscard]] virtual SystemInfo getSystemInfo() const = 0;

    // The theme and the battery as the platform last reported them, which the engine reads once per frame.
    [[nodiscard]] virtual Theme getTheme() const = 0;
    [[nodiscard]] virtual Battery getBattery() const = 0;

    // The accessibility settings as the platform last reported them, which the engine reads once per frame to publish `accessibilitySettingsChanged`.
    [[nodiscard]] virtual AccessibilitySettings getAccessibilitySettings() const = 0;

    // Whether the platform wants the tree of accessible elements, such as while a screen reader runs or since an assistive technology first asked for the elements of the window, which the engine reads once per frame.
    [[nodiscard]] virtual bool isAccessibilityTreeWanted() const = 0;

    // Hands the platform the tree of accessible elements whenever it changed, with the id of the element that holds the focus or 0. The platform copies what it keeps and answers the actions of screen readers through `AccessibilityRelay::perform`, from any thread.
    virtual void updateAccessibility(std::span<const AccessibilityElement> elements, std::uint64_t focused) = 0;

    // Opens a decoder of the video file that the reader reads, which the engine calls on a worker of the I/O pool, so several may open at once. Returns null where the platform has no decoder of video files, and throws `std::runtime_error` for a file the platform cannot decode.
    [[nodiscard]] virtual std::unique_ptr<VideoDecoder> openVideo(std::shared_ptr<io::PackageReader> file, std::string_view path) = 0;

    // Shows what the app plays in the now-playing and lock-screen controls of the system, or takes the app out of them with nothing, where the platform has such controls. The platform sends the commands of the controls, the media keys and headset buttons through `MediaSession::receive`.
    virtual void setNowPlaying(const std::optional<MediaSession::NowPlaying>& value) = 0;

    // Asks the screen reader to say a text, after what it says now or interrupting it, where the platform has a screen reader.
    virtual void announce(std::string_view text, bool interrupt) = 0;

    // Opens the url with the app the system picks for it. The callback runs exactly once, on any thread and possibly after the engine that asked is gone, with whether an app took the url.
    virtual void openUrl(std::string_view url, std::function<void(bool opened)> callback) = 0;

    // Vibrates the device for the given seconds where it can vibrate, and does nothing elsewhere.
    virtual void vibrate(float seconds) = 0;

    // Runs the low and the high frequency motors of a gamepad, each from 0 to 1, for the seconds, where the gamepad and the platform rumble, and does nothing elsewhere. Zero intensities stop the gamepad.
    virtual void rumbleGamepad(std::size_t index, float low, float high, float seconds) = 0;

    // The sentence that engine-owned network errors end with while the project of the app lacks what network access needs on this platform, such as the permission `INTERNET` that an Android app does not declare, or an empty text. The engine reads it when an app starts.
    [[nodiscard]] virtual std::string getNetworkRequirement() const = 0;

    // Shows a native dialog, which the platform answers exactly once through `DialogRelay::resolve` with the same id, from any thread. Picked files that have no path of their own are copied into `folder`, which the platform creates when it needs it.
    virtual void showDialog(std::uint64_t id, const DialogRequest& request, const std::filesystem::path& folder) = 0;

    // Closes a dialog that the app gave up, where the platform can. Its answer is dropped either way.
    virtual void cancelDialog(std::uint64_t id) = 0;

    // Opens the screen of a plugin over the app, which the engine covered already. The platform keeps the id, the plugin, the screen and the state where it keeps them across the end of the process, and ends the screen exactly once through `ScreenRelay::finish` with the same id, from any thread, with its result or a failure, such as `cancelled` when the user closed it, `notActive` when the platform cannot present it now or `unsupported`. A screen that ended after the process did reaches the next app through `ScreenRelay::restore` with the kept state.
    virtual void openScreen(const ScreenRequest& request) = 0;

    // Dismisses a screen that the app gave up, where the platform can, which still ends it through `ScreenRelay::finish` once it is gone.
    virtual void cancelScreen(std::uint64_t id) = 0;

    // Captures the native crashes of the process from now on into the report file, replacing the file of an earlier app of the process, where the platform catches them.
    virtual void guardCrashes(const std::filesystem::path& report) = 0;

    // Publishes the JSON object that the report of a crash carries as its context.
    virtual void setCrashContext(std::string_view json) = 0;

    // The development session of a process that plays an app while it is being made, or null for every app that ships. It outlives the apps that restart on the host.
    [[nodiscard]] virtual DevelopmentSession* getDevelopmentSession() const noexcept = 0;
};

} // namespace haylen::platform
