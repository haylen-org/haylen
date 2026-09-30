#pragma once

#include <span>
#include <vector>

#include "haylen/math/Polygon.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/platform/Monitor.hpp"
#include "haylen/platform/Window.hpp"
#include "platform/WindowStyle.hpp"

struct _XDisplay;

namespace haylen::platform {

// The desktop window of X11. Desktop points are pixels divided by the scale of `Xft.dpi`, which sizes the window of `app.json` too. The input shape of the window lets clicks through outside the regions, and a connection of its own hears the moves of the window and the changes of the monitors, because the event loop of `sokol_app` drops what it does not handle. Window managers apply the hints, and transparency needs a compositing one.
class LinuxDesktop final {
  public:
    static void setStyle(const WindowStyle& value);
    [[nodiscard]] static math::Rect getFrame();
    static void setFrame(const math::Rect& value);
    [[nodiscard]] static std::vector<Monitor> getMonitors();
    static void setPassthrough(Window::Passthrough mode, std::span<const math::Polygon::Outline> value);
    static void startDrag();
    static void watch();
    static void update();
    static void release() noexcept;

  private:
    static constexpr long kMotifDecorations = 2;
    static constexpr long kMoveWindow = 8;
    static constexpr long kFromApplication = 1;

    static _XDisplay* watcher;
    static unsigned long workAreaAtom;
    static int randrEvents;
    static WindowStyle style;
    static Window::Passthrough passthrough;
    static std::vector<math::Polygon::Outline>& regions;
    static bool dragging;

    [[nodiscard]] static _XDisplay* getDisplay();
    [[nodiscard]] static unsigned long getWindow();
    [[nodiscard]] static float getDesktopScale(_XDisplay* display);
    [[nodiscard]] static std::vector<long> readCardinals(_XDisplay* display, unsigned long window, const char* name);
    [[nodiscard]] static math::Rect readWorkArea(_XDisplay* display, float scale);
    [[nodiscard]] static std::vector<Monitor> readMonitors(_XDisplay* display);
    static void changeState(_XDisplay* display, unsigned long window, const char* state, bool enabled);
    static void applySizeHints(_XDisplay* display, unsigned long window, int width, int height);
    static void followDrag(_XDisplay* display);
    static void readEvents();
};

} // namespace haylen::platform
