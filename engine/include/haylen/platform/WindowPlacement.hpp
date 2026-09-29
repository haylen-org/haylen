#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

#include "haylen/core/Json.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/platform/Monitor.hpp"

namespace haylen::platform {

// Where a desktop window opens or moves to, in desktop points: centered on a monitor, at a point of the desktop, or against a side or a corner of the whole area or the work area of a monitor, which the window can also fill along one or both axes. The position of the window section of app.json and window.place in Lua read it.
class WindowPlacement final {
  public:
    enum class Anchor : std::uint8_t {
        Center,
        Top,
        Bottom,
        Left,
        Right,
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight,
    };

    enum class Area : std::uint8_t {
        Work,
        Full,
    };

    enum class Fill : std::uint8_t {
        None,
        Width,
        Height,
        Both,
    };

    // Reads "center", a point {"x": 40, "y": 60}, or an anchored placement such as {"anchor": "bottom", "area": "work", "monitor": "primary", "offset": [0, -8], "fill": "width"}, where the monitor is "primary" or a number from 1. Throws std::invalid_argument for anything else.
    [[nodiscard]] static WindowPlacement fromJson(const core::Json& value);
    [[nodiscard]] core::Json toJson() const;

    // Returns the frame of a window of the given size in desktop points. A monitor number past the last monitor places the window on the primary monitor.
    [[nodiscard]] math::Rect resolve(std::span<const Monitor> monitors, math::Vec2 size) const;

    [[nodiscard]] bool operator==(const WindowPlacement&) const = default;

  private:
    static constexpr std::array<std::pair<std::string_view, Anchor>, 9> kAnchors{{
        {"center", Anchor::Center},
        {"top", Anchor::Top},
        {"bottom", Anchor::Bottom},
        {"left", Anchor::Left},
        {"right", Anchor::Right},
        {"topLeft", Anchor::TopLeft},
        {"topRight", Anchor::TopRight},
        {"bottomLeft", Anchor::BottomLeft},
        {"bottomRight", Anchor::BottomRight},
    }};
    static constexpr std::array<std::pair<std::string_view, Area>, 2> kAreas{{{"work", Area::Work}, {"full", Area::Full}}};
    static constexpr std::array<std::pair<std::string_view, Fill>, 4> kFills{{{"none", Fill::None}, {"width", Fill::Width}, {"height", Fill::Height}, {"both", Fill::Both}}};

    template <typename T, std::size_t Size> [[nodiscard]] static T readName(const core::Json& object, const char* key, const std::array<std::pair<std::string_view, T>, Size>& names, T fallback);
    template <typename T, std::size_t Size> [[nodiscard]] static std::string_view getName(const std::array<std::pair<std::string_view, T>, Size>& names, T value);
    [[nodiscard]] static float readNumber(const core::Json& value, const char* key);
    [[nodiscard]] static const Monitor& findMonitor(std::span<const Monitor> monitors, std::size_t number);

    std::optional<math::Vec2> point;
    Anchor anchor = Anchor::Center;
    Area area = Area::Work;

    // Zero stands for the primary monitor, and other numbers count the monitors from 1.
    std::size_t monitor = 0;
    math::Vec2 offset;
    Fill fill = Fill::None;
};

} // namespace haylen::platform
