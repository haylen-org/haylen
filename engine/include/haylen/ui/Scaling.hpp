#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

#include "haylen/math/Vec2.hpp"

namespace haylen::ui {

// How large the interface draws: in design units like the rest of the app, or at a physical size that the density of the screen decides, both times a factor such as a player setting.
struct Scaling {
    enum class Mode : std::uint8_t {
        Design,
        Physical,
    };

    // The points of the screen one UI unit spans in the physical mode, so the default control height of 64 units is 48 points, and the shortest side of the visible area that the physical mode keeps in UI units, so a layout always has room.
    static constexpr float kPointsPerUnit = 0.75F;
    static constexpr float kMinimumShortSide = 480.0F;
    static constexpr float kMinimumFactor = 0.25F;
    static constexpr float kMaximumFactor = 4.0F;
    static constexpr std::array<std::pair<std::string_view, Mode>, 2> kModes{{
        {"design", Mode::Design},
        {"physical", Mode::Physical},
    }};

    [[nodiscard]] static std::optional<Mode> modeFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view modeName(Mode value) noexcept;

    // Returns the design units one UI unit spans on a screen with the given framebuffer pixels per design unit, framebuffer pixels per point and visible area in design units.
    [[nodiscard]] float resolve(float pixelsPerUnit, float pixelsPerPoint, math::Vec2 visibleSize) const noexcept;

    Mode mode = Mode::Design;
    float factor = 1.0F;
};

} // namespace haylen::ui
