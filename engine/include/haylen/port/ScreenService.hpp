#pragma once

#include <optional>
#include <vector>

#include "haylen/math/Insets.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/platform/Fold.hpp"
#include "haylen/platform/Orientation.hpp"
#include "haylen/platform/Window.hpp"

namespace haylen::port {

// What the screen of a device holds around the app: the safe area, the orientation, the status bar, the gestures of the system at its edges, a fold and cutouts. A port registers it where its screen has any of them, and the engine treats a platform without it as a landscape screen whose whole area is safe, with no status bar, gestures, fold or cutouts.
class ScreenService {
  public:
    ScreenService() = default;
    ScreenService(const ScreenService&) = delete;
    ScreenService& operator=(const ScreenService&) = delete;
    virtual ~ScreenService() = default;

    // The insets of the screen area that the UI must avoid, in framebuffer pixels.
    [[nodiscard]] virtual math::Insets getSafeAreaInsets() = 0;

    // Returns `Landscape` or `Portrait`, and keeps the screen in one orientation where the platform lets the app turn it.
    [[nodiscard]] virtual platform::Orientation getOrientation() = 0;
    virtual void lockOrientation(platform::Orientation value) = 0;

    // Shows or hides the status bar with its content color, and returns its height in framebuffer pixels, or 0 while none shows.
    virtual void setStatusBar(platform::Window::StatusBar value) = 0;
    [[nodiscard]] virtual float getStatusBarHeight() = 0;

    // Applies how the gestures of the system at the edges of the screen answer a swipe.
    virtual void setSystemGestures(platform::Window::SystemGestures value) = 0;

    // The fold of a foldable or dual-screen device across the window, and the cutouts of the screen over the window, as `Window::getFold` and `Window::getCutouts` describe.
    [[nodiscard]] virtual std::optional<platform::Fold> getFold() = 0;
    virtual void getCutouts(std::vector<math::Rect>& cutouts) = 0;
};

} // namespace haylen::port
