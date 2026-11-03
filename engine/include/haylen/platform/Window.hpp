#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/math/Polygon.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/platform/Display.hpp"
#include "haylen/platform/Fold.hpp"
#include "haylen/platform/Orientation.hpp"
#include "haylen/platform/TextInput.hpp"

namespace haylen::platform {

// The native window or canvas the app runs in.
class Window {
  public:
    enum class Cursor : std::uint8_t {
        Default,
        Arrow,
        IBeam,
        Crosshair,
        PointingHand,
        ResizeHorizontal,
        ResizeVertical,
        ResizeDiagonalDown,
        ResizeDiagonalUp,
        ResizeAll,
        NotAllowed,
    };

    static constexpr std::array<std::pair<std::string_view, Cursor>, 11> kCursorNames{{
        {"default", Cursor::Default},
        {"arrow", Cursor::Arrow},
        {"iBeam", Cursor::IBeam},
        {"crosshair", Cursor::Crosshair},
        {"pointingHand", Cursor::PointingHand},
        {"resizeHorizontal", Cursor::ResizeHorizontal},
        {"resizeVertical", Cursor::ResizeVertical},
        {"resizeDiagonalDown", Cursor::ResizeDiagonalDown},
        {"resizeDiagonalUp", Cursor::ResizeDiagonalUp},
        {"resizeAll", Cursor::ResizeAll},
        {"notAllowed", Cursor::NotAllowed},
    }};

    // Where clicks pass through the window to what is behind it: nowhere, everywhere, or everywhere outside the regions that keep the mouse for the app.
    enum class Passthrough : std::uint8_t {
        Off,
        Whole,
        Regions,
    };

    // How the status bar of a phone or tablet shows over the top of the app: hidden, or shown with the light or dark content that the theme of the system picks or that the app asks for.
    enum class StatusBar : std::uint8_t {
        Hidden,
        Auto,
        Light,
        Dark,
    };

    // How the platform answers an app that asks to list every display. Browsers show the screens beyond the one of the page only once the person allows it, and the other platforms list every display from the start.
    enum class DisplayAccess : std::uint8_t {
        Granted,
        Denied,
        Unsupported,
    };

    // Where and how the window goes fullscreen, as `enterFullscreen` describes. A mode with a refresh rate of 0 runs at the highest rate of its size.
    struct Fullscreen {
        std::string display;
        std::optional<Display::Mode> mode;
    };

    virtual ~Window() = default;

    // Resolves the orientation names `landscape`, `portrait` and `any` that `app.json` and Lua share.
    [[nodiscard]] static std::optional<Orientation> orientationFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view orientationName(Orientation value) noexcept;

    // Resolves the status bar names `hidden`, `auto`, `light` and `dark` that `app.json` and Lua share.
    [[nodiscard]] static std::optional<StatusBar> statusBarFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view statusBarName(StatusBar value) noexcept;

    [[nodiscard]] virtual math::Vec2 getFramebufferSize() const noexcept = 0;
    [[nodiscard]] virtual float getDpiScale() const noexcept = 0;
    [[nodiscard]] virtual bool isFullscreen() const noexcept = 0;

    // Leaves fullscreen, or covers the display of the window at the mode that the desktop runs in.
    virtual void setFullscreen(bool value) = 0;

    // Covers the display with the id, or the display of the window while the id is empty, at the mode that the desktop runs in, or exclusively in a mode of the display, which the display runs in while the window holds it. Desktops move the window to the display first, browsers go fullscreen on the screen where the person let the page see every screen, and the other platforms keep the window on its display in the mode that the system picks. Throws `std::invalid_argument` for a display that is not connected or a mode that the display does not list, and `std::runtime_error` where the platform cannot go fullscreen there.
    virtual void enterFullscreen(const Fullscreen& value) = 0;
    [[nodiscard]] virtual bool isResizable() const noexcept = 0;
    virtual void setResizable(bool value) = 0;
    virtual void setTitle(std::string_view value) = 0;
    virtual void setCursor(Cursor value) = 0;
    virtual void setCursorVisible(bool value) = 0;
    virtual void setMouseLocked(bool value) = 0;
    virtual void setKeyboardVisible(bool value) = 0;
    virtual void requestQuit() = 0;

    // Whether a quit closes the app, which desktops, Android and the headless host do, while iPhones, iPads, TVs, Mac Catalyst windows and web pages close only through the system or the browser.
    [[nodiscard]] virtual bool canQuit() const noexcept = 0;

    // Returns `Landscape` or `Portrait`. Desktop windows always count as landscape.
    [[nodiscard]] virtual Orientation getOrientation() const = 0;

    // Keeps the screen in the given orientations where the platform lets an app choose, and does nothing elsewhere.
    virtual void lockOrientation(Orientation value) = 0;

    // The fold of a foldable or dual-screen device across the window where the platform reports one, which Android does through Jetpack WindowManager and browsers through the Viewport Segments and Device Posture APIs, and none elsewhere.
    [[nodiscard]] virtual std::optional<Fold> getFold() const = 0;

    // Shows or hides the status bar where the platform has one, which iPhones, iPads and Android do, and keeps reporting the value that was set elsewhere.
    [[nodiscard]] virtual StatusBar getStatusBar() const noexcept = 0;
    virtual void setStatusBar(StatusBar value) = 0;

    // Keeps the screen on and its saver away while the app shows, or leaves them to the settings of the system. Game controllers keep neither the idle timer of iOS and tvOS nor the screen saver of desktops from running, so an app that players drive with them or that plays without input keeps the screen on.
    [[nodiscard]] virtual bool isScreenKeptOn() const noexcept = 0;
    virtual void setKeepScreenOn(bool value) = 0;

    [[nodiscard]] virtual TextInput& getTextInput() noexcept = 0;

    // Whether the player can point at the screen with a mouse or a touch screen, which a TV remote cannot.
    [[nodiscard]] virtual bool hasPointerDevice() const noexcept = 0;

    // Whether the desktop shows through the transparent pixels of the window. Only a window that opened able to be transparent, which `window.transparent` in `app.json` decides, can turn transparent again after it turned opaque, and turning such a window transparent otherwise throws `std::logic_error`. An opaque window shows alpha 1 everywhere, whatever the app draws.
    [[nodiscard]] virtual bool canBeTransparent() const noexcept = 0;
    [[nodiscard]] virtual bool isTransparent() const noexcept = 0;
    virtual void setTransparent(bool value) = 0;

    // Desktop windows can drop their title bar and border, float above other windows, stay out of the taskbar and the Dock, and refuse the focus, so that clicking them never activates the app or takes the keyboard. Platforms whose window fills the screen keep reporting the values that were set.
    [[nodiscard]] virtual bool isDecorated() const noexcept = 0;
    virtual void setDecorated(bool value) = 0;
    [[nodiscard]] virtual bool isAlwaysOnTop() const noexcept = 0;
    virtual void setAlwaysOnTop(bool value) = 0;
    [[nodiscard]] virtual bool isShownInTaskbar() const noexcept = 0;
    virtual void setShowInTaskbar(bool value) = 0;
    [[nodiscard]] virtual bool isFocusable() const noexcept = 0;
    virtual void setFocusable(bool value) = 0;

    // The content area of the window in desktop points, with y down from the top left corner of the primary monitor. Platforms whose window fills the screen report the screen and ignore changes.
    [[nodiscard]] virtual math::Rect getFrame() const = 0;
    virtual void setFrame(const math::Rect& value) = 0;

    // Lets clicks pass through the whole window, or through all of it outside the regions, polygons in framebuffer pixels where the app keeps the mouse.
    [[nodiscard]] virtual Passthrough getMousePassthrough() const noexcept = 0;
    virtual void setMousePassthrough(Passthrough mode, std::span<const math::Polygon::Outline> regions) = 0;

    // Moves the window with the mouse for as long as the button that just went down stays down, so the player drags the app by its content.
    virtual void startDrag() = 0;

    // Returns every display of the device, at least one, in the order of the platform, as `Display` describes. A browser lists only the screen of the page until the person lets the page see the others.
    [[nodiscard]] virtual std::vector<Display> getDisplays() const = 0;

    // Returns the id of the display that shows the window, the one that holds most of it on desktops.
    [[nodiscard]] virtual std::string getDisplayId() const = 0;

    // Asks the platform to list every display, which browsers ask the person for, and calls back once on the frame thread with the answer. The displays that a grant adds reach the app as a change of the displays.
    virtual void requestDisplays(std::function<void(DisplayAccess access)> callback) = 0;

  private:
    static const std::array<std::pair<std::string_view, Orientation>, 3> kOrientationNames;
    static const std::array<std::pair<std::string_view, StatusBar>, 4> kStatusBarNames;
};

} // namespace haylen::platform
