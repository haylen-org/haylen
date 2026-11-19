#pragma once

#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/platform/Display.hpp"
#include "haylen/platform/WindowPlacement.hpp"

namespace haylen::test {

// The displays of a desktop with two of them, which the tests of windows, placements and displays place their windows on.
class DisplayLayout final {
  public:
    // A primary display with a taskbar at its bottom, and a denser one to its left whose top sits lower.
    [[nodiscard]] static std::vector<platform::Display> twoDisplays() {
        return {
            {.id = "main-id", .name = "main", .bounds = {0.0F, 0.0F, 1920.0F, 1080.0F}, .workArea = {0.0F, 0.0F, 1920.0F, 1040.0F}, .scale = 1.0F, .primary = true},
            {.id = "side-id", .name = "side", .bounds = {-1440.0F, 200.0F, 1440.0F, 900.0F}, .workArea = {-1440.0F, 225.0F, 1440.0F, 875.0F}, .scale = 2.0F, .primary = false},
        };
    }

    [[nodiscard]] static math::Rect place(const char* position, const std::vector<platform::Display>& displays = twoDisplays(), math::Vec2 size = {400.0F, 100.0F}) {
        return platform::WindowPlacement::fromJson(core::Json::parse(position)).resolve(displays, size);
    }
};

} // namespace haylen::test
