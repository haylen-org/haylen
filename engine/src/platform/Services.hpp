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

#include "haylen/core/Json.hpp"
#include "haylen/input/GamepadState.hpp"
#include "haylen/math/Insets.hpp"
#include "haylen/math/Polygon.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/platform/AccessibilityElement.hpp"
#include "haylen/platform/DialogRequest.hpp"
#include "haylen/platform/Fold.hpp"
#include "haylen/platform/MediaSession.hpp"
#include "haylen/platform/Monitor.hpp"
#include "haylen/platform/Orientation.hpp"
#include "haylen/platform/ScreenRequest.hpp"
#include "haylen/platform/SystemInfo.hpp"
#include "haylen/platform/TextInput.hpp"
#include "haylen/platform/VideoDecoder.hpp"
#include "haylen/platform/Window.hpp"
#include "haylen/platform/native/HaylenNative.h"
#include "platform/WindowStyle.hpp"

namespace haylen::io {
class Package;
class PackageReader;
} // namespace haylen::io

namespace haylen::platform {

// Services every platform folder implements for the Sokol runtime. Exactly one implementation is compiled into a runtime build. The platform reports the theme, the battery and the accessibility settings of the device through `SokolHost::getSystemState`, from any thread, when it initializes and whenever the system changes them.
class Services final {
  public:
    [[nodiscard]] static std::string_view getName() noexcept;

    static void initialize();
    static void shutdown() noexcept;

    // Tells the host environment about a failure that stopped the app, beyond the log and the error screen, with the JSON report of `lua::Error::toJson`. The web runtime passes it to the page and to the web parts of plugins, and the other platforms to the native parts of plugins.
    static void reportError(const core::Json& report);

    // The ids of the plugins whose native part the platform loaded.
    [[nodiscard]] static std::vector<std::string> getNativePlugins();

    // Opens the package that ships with the app when the command line names none.
    [[nodiscard]] static std::shared_ptr<io::Package> openBundledPackage();

    [[nodiscard]] static std::filesystem::path getUserDataDirectory(std::string_view identifier);

    // The folder of the local content store of an app, apart from its user data so clearing one never touches the other, in a place the system does not back up.
    [[nodiscard]] static std::filesystem::path getContentStoreDirectory(std::string_view identifier);
    static void persistUserData();

    // Whether the platform has a desktop, where the app runs in a window that moves among other windows, which Windows, macOS and Linux have.
    [[nodiscard]] static bool hasDesktop() noexcept;

    // Gives the window the decorations, the level and the taskbar presence of the style, which also decides whether the player can resize it. Platforms whose window always fills the screen ignore it.
    static void setWindowStyle(const WindowStyle& value);

    // The content area of the window in desktop points, with y down from the top left corner of the primary monitor.
    [[nodiscard]] static math::Rect getWindowFrame();
    static void setWindowFrame(const math::Rect& value);

    // Returns every monitor of the desktop, at least one. It also works before the window opens.
    [[nodiscard]] static std::vector<Monitor> getMonitors();

    // Lets clicks through the window everywhere, or everywhere outside the regions in framebuffer pixels.
    static void setMousePassthrough(Window::Passthrough mode, std::span<const math::Polygon::Outline> regions);

    // Moves the window with the mouse while the button that went down last stays down.
    static void startWindowDrag();

    // Starts following the window once `sokol_app` opened it, so its moves and the changes of the monitors reach the running app as events.
    static void watchWindow();

    // Runs before every frame of the app, where platforms follow the mouse over the window for passthrough and Linux runs the main context of GLib.
    static void updateWindow();

    // Returns the insets of the screen area that UI must avoid, in framebuffer pixels.
    [[nodiscard]] static math::Insets getSafeAreaInsets();

    // Whether the device has a mouse or a touch screen, which TVs lack.
    [[nodiscard]] static bool hasPointerDevice() noexcept;

    static void pollGamepads(std::span<input::GamepadState> gamepads);

    // Returns `Landscape` or `Portrait`. Desktop windows always count as landscape.
    [[nodiscard]] static Orientation getOrientation();
    static void lockOrientation(Orientation value);

    // Shows or hides the status bar with its content color where the platform has one, and returns its height in framebuffer pixels, or 0 while none shows.
    static void setStatusBar(Window::StatusBar value);
    [[nodiscard]] static float getStatusBarHeight();

    // Returns the fold of a foldable or dual-screen device across the window, as `Window::getFold` describes.
    [[nodiscard]] static std::optional<Fold> getFold();

    // Returns the text input of the platform, which lives as long as the process.
    [[nodiscard]] static TextInput& getTextInput();

    // Hands a call to the native handler registry with the byte buffers that its parameters refer to, which the registry copies before it returns, and it answers through the bridge relay. Methods without a handler fail with an error result.
    static void dispatch(std::uint64_t id, std::string_view method, std::string_view paramsJson, std::span<const std::vector<std::byte>> buffers);

    // Tells the native handler of a call that the app gave it up, so it can stop working on it.
    static void cancel(std::uint64_t id);

    // What the device and its operating system are, read once per process. The graphics device names the GPU, so `gpuName` stays empty.
    [[nodiscard]] static SystemInfo getSystemInfo();

    // Opens the url with the app the system picks for it. The callback runs exactly once, on any thread, with whether an app took the url.
    static void openUrl(const std::string& url, std::function<void(bool opened)> callback);

    // Vibrates the device for the given seconds where it can vibrate, and does nothing elsewhere.
    static void vibrate(float seconds);

    // Shows what the app plays in the now-playing and lock-screen controls of the platform, or takes the app out of them, as `Host::setNowPlaying` describes. Linux has no such controls the engine reaches and ignores it.
    static void setNowPlaying(const std::optional<MediaSession::NowPlaying>& value);

    // Exposes the tree of accessible elements to the screen readers of the platform and says announcements, as `Host::isAccessibilityTreeWanted`, `Host::updateAccessibility` and `Host::announce` describe. A platform without a screen reader interface never wants the tree and ignores both.
    [[nodiscard]] static bool isAccessibilityTreeWanted();
    static void updateAccessibility(std::span<const AccessibilityElement> elements, std::uint64_t focused);
    static void announce(std::string_view text, bool interrupt);

    // Opens a decoder of a video file with the decoder of the platform, from any thread, as `Host::openVideo` describes.
    [[nodiscard]] static std::unique_ptr<VideoDecoder> openVideo(std::shared_ptr<io::PackageReader> file, std::string_view path);

    // Runs the motors of a gamepad as `Host::rumbleGamepad` describes.
    static void rumbleGamepad(std::size_t index, float low, float high, float seconds);

    // Shows a native dialog and answers it exactly once through `DialogRelay::resolve` with the same id, from any thread. Picked files that have no path of their own are copied into `folder`, which the platform creates when it needs it.
    static void showDialog(std::uint64_t id, const DialogRequest& request, const std::filesystem::path& folder);

    // Closes a dialog that the app gave up, where the platform can.
    static void cancelDialog(std::uint64_t id);

    // Opens the screen of a plugin and ends it exactly once through `ScreenRelay::finish`, from any thread, as `Host::openScreen` describes, and dismisses a screen that the app gave up where the platform can.
    static void openScreen(const ScreenRequest& request);
    static void cancelScreen(std::uint64_t id);

    // Ends the launch screen that covered the window since the app started, fading it out over the seconds. Platforms without a launch screen of their own do nothing.
    static void endSplashScreen(float fadeOutSeconds);

    // The window of the app on the desktops for native libraries, as `HaylenNativeWindow` describes, and null handles where apps have no desktop window. It exists once `sokol_app` opened the window.
    [[nodiscard]] static HaylenNativeWindow getNativeWindow();
};

} // namespace haylen::platform
