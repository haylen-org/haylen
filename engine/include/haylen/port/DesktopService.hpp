#pragma once

#include <span>

#include "haylen/math/Color.hpp"
#include "haylen/math/Polygon.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/platform/Window.hpp"
#include "haylen/platform/WindowStyle.hpp"
#include "haylen/platform/native/HaylenNative.h"

namespace haylen::port {

// The desktop of a platform where the app runs in a window that moves among other windows, as on Windows, macOS and Linux. A port registers it only when its platform has such a desktop, and the engine treats a platform without it as one whose window fills its screen.
class DesktopService {
  public:
    DesktopService() = default;
    DesktopService(const DesktopService&) = delete;
    DesktopService& operator=(const DesktopService&) = delete;
    virtual ~DesktopService() = default;

    // Gives the window the decorations, the level and the taskbar presence of the style, which also decides whether the player can resize it.
    virtual void setWindowStyle(const platform::WindowStyle& value) = 0;

    // Gives the window the color it shows where the app has not drawn yet, on a system that paints a window itself as it shows it.
    virtual void setWindowBackground(const math::Color& value) = 0;

    // Moves and sizes the content area of the window in desktop points.
    virtual void setWindowFrame(const math::Rect& value) = 0;

    // Lets clicks through the window everywhere, or everywhere outside the regions in framebuffer pixels.
    virtual void setMousePassthrough(platform::Window::Passthrough mode, std::span<const math::Polygon::Outline> regions) = 0;

    // Moves the window with the mouse while the button that went down last stays down.
    virtual void startWindowDrag() = 0;

    // Starts following the window once it opened, so its moves and the changes of the displays reach the app, and runs before every frame of the app.
    virtual void watchWindow() = 0;
    virtual void updateWindow() = 0;

    // The window of the app for native libraries, as `HaylenNativeWindow` describes.
    [[nodiscard]] virtual HaylenNativeWindow getNativeWindow() = 0;
};

} // namespace haylen::port
