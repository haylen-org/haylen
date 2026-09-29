#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

#include "haylen/core/Json.hpp"
#include "haylen/math/Insets.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::platform {

// A safe area the engine uses instead of the one the device reports, to test layouts around notches, dynamic islands, rounded corners, gesture bars and TV overscan without the device. A device preset scales the insets of that device to the window in the orientation of the window, while explicit insets are window points.
class SafeAreaSimulation final {
  public:
    // A device the simulation imitates: its screen in portrait and its insets in each orientation, in points.
    struct Device {
        std::string_view name;
        math::Vec2 screen;
        math::Insets portrait;
        math::Insets landscape;
    };

    static constexpr std::array<Device, 5> kDevices{{
        {"iphoneNotch", {390.0F, 844.0F}, {.left = 0.0F, .top = 47.0F, .right = 0.0F, .bottom = 34.0F}, {.left = 47.0F, .top = 0.0F, .right = 47.0F, .bottom = 21.0F}},
        {"iphoneDynamicIsland", {393.0F, 852.0F}, {.left = 0.0F, .top = 59.0F, .right = 0.0F, .bottom = 34.0F}, {.left = 59.0F, .top = 0.0F, .right = 59.0F, .bottom = 21.0F}},
        {"ipad", {820.0F, 1180.0F}, {.left = 0.0F, .top = 24.0F, .right = 0.0F, .bottom = 20.0F}, {.left = 0.0F, .top = 24.0F, .right = 0.0F, .bottom = 20.0F}},
        {"androidGestureBar", {412.0F, 915.0F}, {.left = 0.0F, .top = 32.0F, .right = 0.0F, .bottom = 24.0F}, {.left = 32.0F, .top = 0.0F, .right = 0.0F, .bottom = 24.0F}},
        {"television", {1080.0F, 1920.0F}, {.left = 60.0F, .top = 80.0F, .right = 60.0F, .bottom = 80.0F}, {.left = 80.0F, .top = 60.0F, .right = 80.0F, .bottom = 60.0F}},
    }};

    // Reads a device name such as "iphoneDynamicIsland", or insets in window points as one number, two numbers for the vertical and horizontal sides, or four numbers from the top clockwise. Throws std::invalid_argument for anything else.
    [[nodiscard]] static SafeAreaSimulation fromJson(const core::Json& value);

    // Returns the insets in framebuffer pixels for a framebuffer of the given size and pixels per point.
    [[nodiscard]] math::Insets getInsets(math::Vec2 framebuffer, float dpiScale) const noexcept;

    // Writes the device name or the four insets from the top clockwise.
    [[nodiscard]] core::Json toJson() const;

    [[nodiscard]] bool operator==(const SafeAreaSimulation&) const = default;

  private:
    [[nodiscard]] static float readSide(const core::Json& value);

    std::optional<std::size_t> device;
    math::Insets insets;
};

} // namespace haylen::platform
