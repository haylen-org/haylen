#pragma once

#include <functional>
#include <string>
#include <vector>

#include "haylen/math/Rect.hpp"
#include "haylen/platform/Display.hpp"
#include "haylen/platform/Permission.hpp"

namespace haylen::port {

// The displays of the device and where the window shows among them. Every port registers it, since every platform has at least the display that shows the app, and its methods may run before the window opens.
class DisplayService {
  public:
    DisplayService() = default;
    DisplayService(const DisplayService&) = delete;
    DisplayService& operator=(const DisplayService&) = delete;
    virtual ~DisplayService() = default;

    // Every display of the device, at least one, with ids that stay the same while a display stays connected, as `Window::getDisplays` describes.
    [[nodiscard]] virtual std::vector<platform::Display> getDisplays() = 0;

    // The id of the display that shows the window.
    [[nodiscard]] virtual std::string getWindowDisplayId() = 0;

    // Whether the device has displays that `getDisplays` leaves out until the person lets the app see every screen, as a browser tells before `requestEveryScreen`.
    [[nodiscard]] virtual bool hasUnlistedDisplays() = 0;

    // Asks the platform to list every display of the device, and calls back once, on any thread, with `Granted` once `getDisplays` holds them, or `Denied` or `Unsupported` with the sentence that says why. A platform that lists every display already answers `Granted` at once.
    virtual void requestEveryScreen(std::function<void(platform::Permission::Answer answer)> callback) = 0;

    // The content area of the window in points of the display, with y down from the top left corner of the primary display, which is the whole display where the window fills it.
    [[nodiscard]] virtual math::Rect getWindowFrame() = 0;
};

} // namespace haylen::port
