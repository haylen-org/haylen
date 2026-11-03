#pragma once

#include <span>
#include <string>
#include <vector>

#include "haylen/math/Rect.hpp"

namespace haylen::platform {

// A display that the device shows pictures on: a monitor of a desktop, the screen of a phone, a tablet or a TV, an external display attached to one, or a screen that a browser lets the page see. The id stays the same while the display stays connected, and across runs where the platform knows the display again, while the place of the display in the list changes as others connect and disconnect. The bounds cover the whole display and the work area is the part apps may use, without the taskbar, the Dock, the menu bar or the bars of the system, both in points. The displays of a desktop share its points, with y down from the top left corner of the primary display, while each display of a platform without a desktop has its own points from its top left corner. The scale is the number of pixels in a point, and a refresh rate of 0 means that the platform does not tell it.
struct Display {
    // A mode that the display can run in for exclusive fullscreen, in pixels.
    struct Mode {
        int width = 0;
        int height = 0;
        float refreshRate = 0.0F;
        float scale = 1.0F;

        [[nodiscard]] bool operator==(const Mode&) const = default;
    };

    std::string id;
    std::string name;
    math::Rect bounds;
    math::Rect workArea;
    float scale = 1.0F;
    float refreshRate = 0.0F;
    bool primary = false;
    std::vector<Mode> modes;

    // Returns the mode of the size in pixels at the refresh rate, within a hundredth of a frame per second, or at the highest rate of the size for a rate of 0, or null when the display lists no such mode.
    [[nodiscard]] const Mode* findMode(int width, int height, float rate) const noexcept;

    // Returns the display of a list of at least one that holds most of a frame in desktop points, or the primary display while the frame is off every display.
    [[nodiscard]] static const Display& findHolding(std::span<const Display> displays, const math::Rect& frame);

    [[nodiscard]] bool operator==(const Display&) const = default;
};

} // namespace haylen::platform
