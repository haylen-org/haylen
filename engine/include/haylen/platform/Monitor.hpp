#pragma once

#include <string>

#include "haylen/math/Rect.hpp"

namespace haylen::platform {

// A display of the desktop. The bounds cover the whole display and the work area leaves out the taskbar, the Dock and the menu bar, both in desktop points with y down from the top left corner of the primary monitor. The scale is the number of pixels in a point.
struct Monitor {
    std::string name;
    math::Rect bounds;
    math::Rect workArea;
    float scale = 1.0F;
    bool primary = false;

    [[nodiscard]] bool operator==(const Monitor&) const = default;
};

} // namespace haylen::platform
