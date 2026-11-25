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
#include "haylen/platform/SystemAnswer.hpp"
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

    // How the mouse behaves over the window: a visible pointer that moves freely, a hidden one that moves freely, a hidden one that stays in place and reports only its movement, as a camera that the mouse turns needs, or a pointer that stays inside the window, visible or hidden for an app that draws its own cursor, as a game that scrolls at the edges does.
    enum class MouseMode : std::uint8_t {
        Normal,
        Hidden,
        Locked,
        Confined,
        ConfinedHidden,
    };

    static constexpr std::array<std::pair<std::string_view, MouseMode>, 5> kMouseModeNames{{
        {"normal", MouseMode::Normal},
        {"hidden", MouseMode::Hidden},
        {"locked", MouseMode::Locked},
        {"confined", MouseMode::Confined},
        {"confinedHidden", MouseMode::ConfinedHidden},
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

    // How the system gestures at the edges of a phone or tablet answer a swipe: at once with the home indicator showing, only on a second swipe so the app keeps the first one, or at once with a home indicator that hides until the person touches the screen.
    enum class SystemGestures : std::uint8_t {
        System,
        Deferred,
        AutoHideIndicator,
    };

    // When the app updates and draws: in every frame, as a game does, or only in the frames that something changed in, such as input, an animation, a tween, a timer that fired or work of the event loop, as an app of forms and lists does, which keeps the picture it drew on the screen and saves the GPU and the battery in between.
    enum class Redraw : std::uint8_t {
        Always,
        OnChange,
    };

    // Where and how the window goes fullscreen, as `enterFullscreen` describes. A mode with a refresh rate of 0 runs at the highest rate of its size.
    struct Fullscreen {
        std::string display;
        std::optional<Display::Mode> mode;
    };

    // The highest frame rate an app may aim for, in frames per second.
    static constexpr int kMaxFrameRate = 1000;

    virtual ~Window() = default;

    // Throws `std::invalid_argument` for a frame rate below 0 or above `kMaxFrameRate`.
    static void requireFrameRate(int value);

    // Throws `std::invalid_argument` for a pixel density cap that is not 0 or a positive finite number.
    static void requireMaxPixelDensity(float value);

    // Resolves the orientation names `landscape`, `portrait` and `any` that `haylen.json` and Lua share.
    [[nodiscard]] static std::optional<Orientation> orientationFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view orientationName(Orientation value) noexcept;

    // Resolves the status bar names `hidden`, `auto`, `light` and `dark` that `haylen.json` and Lua share.
    [[nodiscard]] static std::optional<StatusBar> statusBarFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view statusBarName(StatusBar value) noexcept;

    // Resolves the system gesture names `system`, `deferred` and `autoHideIndicator` that `haylen.json` and Lua share.
    [[nodiscard]] static std::optional<SystemGestures> systemGesturesFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view systemGesturesName(SystemGestures value) noexcept;

    // Resolves the redraw names `always` and `onChange` that `haylen.json` and Lua share.
    [[nodiscard]] static std::optional<Redraw> redrawFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view redrawName(Redraw value) noexcept;

    // The name of a mouse mode as Lua writes it, such as `confinedHidden`.
    [[nodiscard]] static std::string_view mouseModeName(MouseMode value) noexcept;

    [[nodiscard]] virtual math::Vec2 getFramebufferSize() const noexcept = 0;
    [[nodiscard]] virtual float getDpiScale() const noexcept = 0;
    [[nodiscard]] virtual bool isFullscreen() const noexcept = 0;

    // Leaves fullscreen, or covers the display of the window at the mode that the desktop runs in. The callback runs exactly once on the frame thread, during the call or at a later frame, with `Granted` once the window is fullscreen or left it, or `Denied` or `Unsupported` with the sentence that says why, such as a browser that goes fullscreen only shortly after an input of the person. A newer change replaces one that the window has not finished, which answers `Denied`, as does a change that the window closes before. An empty callback means that the caller does not want the answer, which then reaches nobody.
    void setFullscreen(bool value, std::function<void(SystemAnswer answer)> callback);

    // Covers the display with the id, or the display of the window while the id is empty, at the mode that the desktop runs in, or exclusively in a mode of the display, which the display runs in while the window holds it. Desktops move the window to the display first, browsers go fullscreen on the screen where the person let the page see every screen, and the other platforms keep the window on its display in the mode that the system picks. Throws `std::invalid_argument` for a display that is not connected or a mode that the display does not list. It answers like `setFullscreen`, with `Unsupported` where the platform cannot go fullscreen there.
    void enterFullscreen(const Fullscreen& value, std::function<void(SystemAnswer answer)> callback);
    [[nodiscard]] virtual bool isResizable() const noexcept = 0;
    virtual void setResizable(bool value) = 0;
    virtual void setTitle(std::string_view value) = 0;
    virtual void setCursor(Cursor value) = 0;

    // The mode of the mouse over the window, which changes once the platform granted a request of `setMouseMode`.
    [[nodiscard]] MouseMode getMouseMode() const noexcept;

    // Puts the mouse in the mode and answers like `setFullscreen`, while a request that the platform grants after a newer one leaves the mode to the newer one. Every platform grants `Normal`. Desktops grant every mode, and a mode that holds the pointer lets it go while the window has no focus and holds it again once the window gets it back. Browsers hide the pointer over the canvas, lock it only during a click or a key of the person and keep no pointer inside the page, iPads and Mac Catalyst windows hide the pointer and lock it where the system lets the app, such as an app that fills the screen, and phones, TVs and headsets have no pointer that an app controls, so the modes that they cannot keep answer `Unsupported`.
    void setMouseMode(MouseMode value, std::function<void(SystemAnswer answer)> callback);
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

    // Writes into `cutouts` the parts of the window that cutouts of the screen hide, such as the camera housing of a notch, a dynamic island or the hole of a camera, in framebuffer pixels: the regions of UIKit on iPhones from iOS 27.1, the cutouts of Android and the housing of the camera of a notched Mac. Other platforms report none, and the vector keeps its storage.
    virtual void getCutouts(std::vector<math::Rect>& cutouts) const = 0;

    // Shows or hides the status bar where the platform has one, which iPhones, iPads and Android do, and keeps reporting the value that was set elsewhere.
    [[nodiscard]] virtual StatusBar getStatusBar() const noexcept = 0;
    virtual void setStatusBar(StatusBar value) = 0;

    // Lets the system gestures at the edges of the screen act at once or defers them to a second swipe, and hides the home indicator until a touch, where the platform has such gestures, which iPhones, iPads and Android do, and keeps reporting the value that was set elsewhere.
    [[nodiscard]] virtual SystemGestures getSystemGestures() const noexcept = 0;
    virtual void setSystemGestures(SystemGestures value) = 0;

    // Keeps the screen on and its saver away while the app shows, or leaves them to the settings of the system, and answers like `setFullscreen`, with `Denied` where a browser refuses its wake lock, such as for a page that hides. Game controllers keep neither the idle timer of iOS and tvOS nor the screen saver of desktops from running, so an app that players drive with them or that plays without input keeps the screen on.
    [[nodiscard]] virtual bool isScreenKeptOn() const noexcept = 0;
    void setKeepScreenOn(bool value, std::function<void(SystemAnswer answer)> callback);

    // The frames per second the app aims for, or 0 for a frame at every refresh of the display, at the highest rate the display shows. Displays that change their refresh, such as those of iPhones, iPads and Android devices, slow down to the rate, and every platform plays a frame every few refreshes when the display refreshes faster, so the frames average the rate. A rate above the refresh of the display plays every refresh while vsync is on, and caps the frames of a window without vsync, which keeps a simple app from spending a whole core. Throws `std::invalid_argument` as `requireFrameRate` does.
    [[nodiscard]] virtual int getFrameRate() const noexcept = 0;
    virtual void setFrameRate(int value) = 0;

    // The most framebuffer pixels that the app draws in each point of the window, or 0 for every pixel of the display. On a denser display the framebuffer shrinks to the cap and the system or the GPU device stretches it over the window, which trades sharpness for fill rate, while the window keeps its size and every size in points and design units stays the same. Throws `std::invalid_argument` as `requireMaxPixelDensity` does.
    [[nodiscard]] virtual float getMaxPixelDensity() const noexcept = 0;
    virtual void setMaxPixelDensity(float value) = 0;

    [[nodiscard]] virtual TextInput& getTextInput() noexcept = 0;

    // Whether the player can point at the screen with a mouse or a touch screen, which a TV remote cannot.
    [[nodiscard]] virtual bool hasPointerDevice() const noexcept = 0;

    // Whether a held key of the keyboard repeats as key-down events whose `repeat` is set, at the delay and the rate of the settings of the system, which desktops, browsers and Android do. The hardware keyboards of iPhones, iPads, Mac Catalyst windows, TVs and headsets report presses and releases alone, so the UI repeats a held key there itself.
    [[nodiscard]] virtual bool hasKeyRepeats() const noexcept = 0;

    // Whether the desktop shows through the transparent pixels of the window. Only a window that opened able to be transparent, which `window.transparent` in `haylen.json` decides, can turn transparent again after it turned opaque, and turning such a window transparent otherwise throws `std::logic_error`. An opaque window shows alpha 1 everywhere, whatever the app draws.
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

    // Whether the device has displays that `getDisplays` leaves out until the person lets the app see every screen, such as the other screens of a browser, which tells that the screen of the page is not the only one before the person lets the page see them.
    [[nodiscard]] virtual bool hasUnlistedDisplays() const = 0;

    // Asks to list every display of the device, which a browser does once the person lets the page see every screen through the Window Management API, and calls back with `Granted` once `getDisplays` holds them, or with `Denied` or `Unsupported` and the sentence that says why. Every other platform lists every display already and answers `Granted`.
    virtual void requestEveryScreen(std::function<void(SystemAnswer answer)> callback) = 0;

  protected:
    // What `setFullscreen`, `enterFullscreen`, `setMouseMode` and `setKeepScreenOn` do on the platform, which always receive a callback.
    virtual void applyFullscreen(bool value, std::function<void(SystemAnswer answer)> callback) = 0;
    virtual void applyFullscreenTarget(const Fullscreen& value, std::function<void(SystemAnswer answer)> callback) = 0;
    virtual void applyMouseMode(MouseMode value, std::function<void(SystemAnswer answer)> callback) = 0;
    virtual void applyKeepScreenOn(bool value, std::function<void(SystemAnswer answer)> callback) = 0;

  private:
    [[nodiscard]] static std::function<void(SystemAnswer answer)> answerTo(std::function<void(SystemAnswer answer)> callback);

    static const std::array<std::pair<std::string_view, Orientation>, 3> kOrientationNames;
    static const std::array<std::pair<std::string_view, StatusBar>, 4> kStatusBarNames;
    static const std::array<std::pair<std::string_view, SystemGestures>, 3> kSystemGesturesNames;
    static const std::array<std::pair<std::string_view, Redraw>, 2> kRedrawNames;

    MouseMode mouseMode = MouseMode::Normal;
    std::uint64_t mouseModeRequest = 0;
    std::uint64_t mouseModeGranted = 0;
};

} // namespace haylen::platform
