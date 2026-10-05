#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/math/Polygon.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/platform/Monitor.hpp"
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

    virtual ~Window() = default;

    // Resolves the orientation names `landscape`, `portrait` and `any` that `app.json` and Lua share.
    [[nodiscard]] static std::optional<Orientation> orientationFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view orientationName(Orientation value) noexcept;

    [[nodiscard]] virtual math::Vec2 getFramebufferSize() const noexcept = 0;
    [[nodiscard]] virtual float getDpiScale() const noexcept = 0;
    [[nodiscard]] virtual bool isFullscreen() const noexcept = 0;
    virtual void setFullscreen(bool value) = 0;
    [[nodiscard]] virtual bool isResizable() const noexcept = 0;
    virtual void setResizable(bool value) = 0;
    virtual void setTitle(std::string_view value) = 0;
    virtual void setCursor(Cursor value) = 0;
    virtual void setCursorVisible(bool value) = 0;
    virtual void setMouseLocked(bool value) = 0;
    virtual void setKeyboardVisible(bool value) = 0;
    virtual void setClipboard(std::string_view value) = 0;
    [[nodiscard]] virtual std::string getClipboard() const = 0;
    virtual void requestQuit() = 0;

    // Returns `Landscape` or `Portrait`. Desktop windows always count as landscape.
    [[nodiscard]] virtual Orientation getOrientation() const = 0;

    // Keeps the screen in the given orientations where the platform lets an app choose, and does nothing elsewhere.
    virtual void lockOrientation(Orientation value) = 0;

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

    // Returns every monitor of the desktop. Platforms whose window fills the screen report the screen as the only monitor.
    [[nodiscard]] virtual std::vector<Monitor> getMonitors() const = 0;

    // Returns the monitor that holds most of the window, or the primary monitor while the window is off every monitor.
    [[nodiscard]] Monitor getCurrentMonitor() const;

  private:
    static const std::array<std::pair<std::string_view, Orientation>, 3> kOrientationNames;
};

} // namespace haylen::platform
