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

#include "haylen/audio/Mixer.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/gpu/Device.hpp"
#include "haylen/input/GamepadState.hpp"
#include "haylen/input/MotionSensors.hpp"
#include "haylen/input/MotionState.hpp"
#include "haylen/math/Insets.hpp"
#include "haylen/platform/AccessibilityElement.hpp"
#include "haylen/platform/AccessibilitySettings.hpp"
#include "haylen/platform/Battery.hpp"
#include "haylen/platform/DialogRequest.hpp"
#include "haylen/platform/MediaSession.hpp"
#include "haylen/platform/Output.hpp"
#include "haylen/platform/Permission.hpp"
#include "haylen/platform/ScreenRequest.hpp"
#include "haylen/platform/SystemInfo.hpp"
#include "haylen/platform/Theme.hpp"
#include "haylen/platform/VideoDecoder.hpp"
#include "haylen/platform/Window.hpp"
#include "varn/dataview/DataView.h"

namespace haylen::io {
class PackageReader;
}

namespace haylen::platform {

class DevelopmentSession;

// Boundary between the portable engine and a platform. The hosts of the platform layers implement it for real devices, the one of SDL and the one of Android, and the headless host implements it for tests.
class Host : public Window {
  public:
    // Explains, from any thread, why the platform blocked the connection of a socket, given its descriptor, or answers an empty text when it blocked nothing or cannot tell.
    using BlockedReason = std::function<std::string(std::intptr_t socket)>;

    // What the platform reports of an output that `openOutput` opened: whether it ended and why, with a sentence for an output that the platform cannot show, and while it shows, the surface that the GPU device draws into, with the size of its drawable in pixels, and the pixels in a point of its display.
    struct OutputReport {
        std::optional<Output::Ending> ending;
        std::string reason;
        std::optional<gpu::Surface> surface;
        float scale = 1.0F;
    };

    [[nodiscard]] virtual std::string_view getPlatformName() const noexcept = 0;
    // Creates the GPU device, which draws into the window of the host. One exists while an app runs.
    [[nodiscard]] virtual std::unique_ptr<gpu::Device> createGpuDevice() = 0;
    [[nodiscard]] virtual audio::Mixer::Setup getAudioSetup() const = 0;

    // Whether the window composes the alpha of every frame with the desktop.
    [[nodiscard]] virtual bool isFrameTransparent() const noexcept = 0;
    [[nodiscard]] virtual std::filesystem::path getUserDataDirectory(std::string_view identifier) = 0;
    virtual void persistUserData() = 0;
    [[nodiscard]] virtual math::Insets getSafeAreaInsets() const = 0;

    // The height in framebuffer pixels of the status bar that shows over the top of the app, or 0 while none shows.
    [[nodiscard]] virtual float getStatusBarHeight() const = 0;

    virtual void pollGamepads(std::span<input::GamepadState> gamepads) = 0;

    // The motion sensors that the device has, which the engine reads before it starts any of them.
    [[nodiscard]] virtual input::MotionSensors::Support getMotionSupport() = 0;

    // Runs the motion sensors of the device in the set at the readings per second, which the platform keeps within what its hardware delivers, or stops every one of them with an empty set.
    virtual void setMotion(input::MotionSensors::Sensors sensors, float rate) = 0;

    // Copies the newest readings of the sensors that run into the state, in the axes of the screen as the app shows it, with the sensors that delivered a reading since they started.
    virtual void readMotion(input::MotionState& state) = 0;

    // Runs the motion sensors in the set on every gamepad that has them, also on those that connect later, or stops them with an empty set. Their readings arrive with the gamepads.
    virtual void setGamepadMotion(input::MotionSensors::Sensors sensors) = 0;

    // Puts the text on the clipboard of the system and calls back with `Granted` once it got there, or `Denied` or `Unsupported` with the sentence that says why. Browsers write the clipboard of a page asynchronously and refuse a page without a recent input of the person or outside a secure context, so their answer comes later, while the other platforms answer during the call.
    virtual void setClipboard(std::string_view value, std::function<void(Permission::Answer answer)> callback) = 0;

    // The text on the clipboard for a paste of the person into a field of the UI, read during the call.
    [[nodiscard]] virtual std::string getClipboard() const = 0;

    // Reads the text on the clipboard for the app, which iOS, macOS and browsers may let the app read only once the person allows it in a prompt that the read shows. The callback runs exactly once, on any thread and possibly after the engine that asked is gone, with `Granted` and the text, or `Denied` or `Unsupported` with the sentence that says why.
    virtual void readClipboard(std::function<void(Permission::Answer answer, std::string text)> callback) = 0;

    // Hands a platform call to the handlers of the platform, with the byte buffers that its parameters refer to, which stay valid until it returns.
    virtual void dispatchPlatformCall(std::uint64_t id, std::string_view method, std::string_view paramsJson, std::span<const std::vector<std::byte>> buffers, std::span<const varn::dataview::DataView> views) = 0;
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

    // Opens the url with the app the system picks for it. The callback runs exactly once, on any thread and possibly after the engine that asked is gone, with `Granted` once an app took the url, `Unsupported` when no app opens it or `Denied` when the platform refused it, such as a browser that blocks a tab without a recent input of the person.
    virtual void openUrl(std::string_view url, std::function<void(Permission::Answer answer)> callback) = 0;

    // Vibrates the device for the given seconds and answers `Granted`, or `Denied` or `Unsupported` with the sentence that says why, such as a device without a motor or an Android app without the permission `VIBRATE`.
    [[nodiscard]] virtual Permission::Answer vibrate(float seconds) = 0;

    // Runs the low and the high frequency motors of a gamepad, each from 0 to 1, for the seconds, where the gamepad and the platform rumble, and does nothing elsewhere. Zero intensities stop the gamepad.
    virtual void rumbleGamepad(std::size_t index, float low, float high, float seconds) = 0;

    // Reads the state of a permission without asking the person, which some platforms tell only asynchronously, such as browsers. The callback runs exactly once, on any thread and possibly after the engine that asked is gone.
    virtual void checkPermission(Permission::Kind kind, std::function<void(Permission::Answer answer)> callback) = 0;

    // Asks the person for a permission with the prompt of the system where the platform has one, after it checked what the project of the app must declare for it. The callback runs exactly once, on any thread and possibly after the engine that asked is gone, with the state the person left, or `Unsupported` with the reason and the requirements the project lacks.
    virtual void requestPermission(Permission::Kind kind, std::function<void(Permission::Answer answer)> callback) = 0;

    // The explanation of blocked sockets that the connections the engine opens itself read when they fail, such as the Local Network Protection of Android. It never refers to the host, so connections that outlive the host may still call it.
    [[nodiscard]] virtual BlockedReason getBlockedReason() const = 0;

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

    // Whether the Lua debugger may halt the frame thread while the platform keeps running, which a web page cannot, since it runs the app on the thread that delivers every message.
    [[nodiscard]] virtual bool canHaltFrames() const noexcept = 0;

    // Keeps the platform responsive while the Lua debugger halts the frame thread: it processes the pending window and system events, which reach the app only once it goes on, and returns whether the platform asks the frame to finish, such as a quit or a window that goes away.
    [[nodiscard]] virtual bool pumpHaltedEvents() = 0;

    // Opens an output on a display that does not show the window, which the platform announces with `Event::Type::OutputChanged` for its number whenever its report changes: once it shows with its surface, when the surface resizes, goes away or comes back, and once it ends. The report keeps the surface of the last announcement until the frame thread reads the next one, and a platform without outputs on that display ends the output with `Output::Ending::Unsupported` and its reason.
    virtual void openOutput(std::uint32_t number, std::string_view display, const Output::Options& options) = 0;

    // Closes an output that the app gave up or whose ending the frame thread read, which ends without another announcement.
    virtual void closeOutput(std::uint32_t number) = 0;
    [[nodiscard]] virtual OutputReport getOutputReport(std::uint32_t number) const = 0;

    // Whether the app that plays was launched by another app of the process, which starts again when it quits.
    [[nodiscard]] virtual bool isLaunched() const noexcept = 0;

    // Whether the person holds the back button of the platform down, such as the back button of a TV remote, which apps hear only as presses.
    [[nodiscard]] virtual bool isBackHeld() const noexcept = 0;
};

} // namespace haylen::platform
