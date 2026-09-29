#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "haylen/math/Vec2.hpp"
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

    virtual ~Window() = default;

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

    // Returns Landscape or Portrait. Desktop windows always count as landscape.
    [[nodiscard]] virtual Orientation getOrientation() const = 0;

    // Keeps the screen in the given orientations where the platform lets an app choose, and does nothing elsewhere.
    virtual void lockOrientation(Orientation value) = 0;

    [[nodiscard]] virtual TextInput& getTextInput() noexcept = 0;

    // Whether the player can point at the screen with a mouse or a touch screen, which a TV remote cannot.
    [[nodiscard]] virtual bool hasPointerDevice() const noexcept = 0;
};

} // namespace haylen::platform
