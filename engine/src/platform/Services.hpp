#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "haylen/input/GamepadState.hpp"
#include "haylen/math/Insets.hpp"
#include "haylen/math/Polygon.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/platform/Monitor.hpp"
#include "haylen/platform/Orientation.hpp"
#include "haylen/platform/TextInput.hpp"
#include "haylen/platform/Window.hpp"
#include "platform/WindowStyle.hpp"

namespace haylen::io {
class Package;
}

namespace haylen::lua {
class Error;
}

namespace haylen::platform {

// Services every platform folder implements for the Sokol runtime. Exactly one implementation is compiled into a runtime build.
class Services final {
  public:
    [[nodiscard]] static std::string_view getName() noexcept;

    static void initialize();
    static void shutdown() noexcept;

    // Tells the host environment about a failure that stopped the app, beyond the log and the error screen. The web runtime passes it to the page with its stack.
    static void reportError(const lua::Error& error);

    // Opens the package that ships with the app when the command line names none.
    [[nodiscard]] static std::shared_ptr<io::Package> openBundledPackage();

    [[nodiscard]] static std::filesystem::path getUserDataDirectory(std::string_view identifier);
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

    // Starts following the window once sokol_app opened it, so its moves and the changes of the monitors reach the running app as events.
    static void watchWindow();

    // Runs before every frame of the app, where platforms follow the mouse over the window for passthrough.
    static void updateWindow();

    // Returns the insets of the screen area that UI must avoid, in framebuffer pixels.
    [[nodiscard]] static math::Insets getSafeAreaInsets();

    // Whether the device has a mouse or a touch screen, which TVs lack.
    [[nodiscard]] static bool hasPointerDevice() noexcept;

    static void pollGamepads(std::span<input::GamepadState> gamepads);

    // Returns Landscape or Portrait. Desktop windows always count as landscape.
    [[nodiscard]] static Orientation getOrientation();
    static void lockOrientation(Orientation value);

    // Returns the text input of the platform, which lives as long as the process.
    [[nodiscard]] static TextInput& getTextInput();

    // Hands a call to the native handler registry, which answers through the bridge relay. Methods without a handler fail with an error result.
    static void dispatch(std::uint64_t id, std::string_view method, std::string_view paramsJson);

    // Tells the native handler of a call that the app gave it up, so it can stop working on it.
    static void cancel(std::uint64_t id);
};

} // namespace haylen::platform
