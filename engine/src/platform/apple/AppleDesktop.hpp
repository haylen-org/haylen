#pragma once

#include <TargetConditionals.h>

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>

#include <span>
#include <vector>

#include "haylen/math/Polygon.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/platform/Monitor.hpp"
#include "haylen/platform/Window.hpp"
#include "platform/WindowStyle.hpp"

namespace haylen::platform {

// The desktop window of macOS. AppKit counts screen coordinates from the bottom of the primary screen, so frames flip against it. A window takes or ignores the mouse as a whole, so passthrough follows the mouse every frame and lets the window ignore it outside the regions, while the app keeps hearing the mouse move.
class AppleDesktop final {
  public:
    static void setStyle(const WindowStyle& value);
    [[nodiscard]] static math::Rect getFrame();
    static void setFrame(const math::Rect& value);
    [[nodiscard]] static std::vector<Monitor> getMonitors();
    static void setPassthrough(Window::Passthrough mode, std::span<const math::Polygon::Outline> value);
    static void startDrag();
    static void watch();
    static void update();

  private:
    static NSEvent* lastMouseDown;
    static Window::Passthrough passthrough;
    static std::vector<math::Polygon::Outline>& regions;
    static math::Vec2 pointer;
    static bool pointerInside;
    static bool dragging;

    [[nodiscard]] static NSWindow* getWindow();
    [[nodiscard]] static math::Rect toDesktop(NSRect rect);
    [[nodiscard]] static NSRect toScreen(const math::Rect& rect);
    [[nodiscard]] static bool isOverRegion(math::Vec2 pixel);
    static void followPointer(math::Vec2 pixel, bool inside);
    static void finishDrag(math::Vec2 pixel);
};

} // namespace haylen::platform
#endif
