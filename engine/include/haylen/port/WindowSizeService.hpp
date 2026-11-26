#pragma once

#include "haylen/math/Vec2.hpp"

namespace haylen::port {

// The size of a window that the app may ask for on a platform without a desktop to place it on, such as a window in the space around the person. A port registers it only where such a window exists, and the engine treats a platform without it and without a desktop as one whose window fills its screen and keeps its size.
class WindowSizeService {
  public:
    WindowSizeService() = default;
    WindowSizeService(const WindowSizeService&) = delete;
    WindowSizeService& operator=(const WindowSizeService&) = delete;
    virtual ~WindowSizeService() = default;

    // Asks the system for the size of the window in points, which the system may change within its own limits. The window reports the size it took as it resizes.
    virtual void requestSize(math::Vec2 points) = 0;
};

} // namespace haylen::port
