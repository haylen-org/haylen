#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "haylen/core/Json.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/platform/Display.hpp"

namespace haylen::platform {

// Where a desktop window opens or moves to, in desktop points: centered on a display, at a point of the desktop, or against a side or a corner of the whole area or the work area of a display, which the window can also fill along one or both axes. The position of the window section of `app.json` and `window.place` in Lua read it.
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

    // Reads `"center"`, a point `{"x": 40, "y": 60}`, or an anchored placement such as `{"anchor": "bottom", "area": "work", "display": "primary", "offset": [0, -8], "fill": "width"}`, where the display is `"primary"`, a number from 1 in the order of the displays or the id of a display. Throws `std::invalid_argument` for anything else.
    [[nodiscard]] static WindowPlacement fromJson(const core::Json& value);
    [[nodiscard]] core::Json toJson() const;

    // Returns the frame of a window of the given size in desktop points. A number past the last display, or the id of a display that is not connected, places the window on the primary display.
    [[nodiscard]] math::Rect resolve(std::span<const Display> displays, math::Vec2 size) const;

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
    [[nodiscard]] const Display& findDisplay(std::span<const Display> displays) const;

    std::optional<math::Vec2> point;
    Anchor anchor = Anchor::Center;
    Area area = Area::Work;

    // The display by its number from 1 or by its id, and the primary display while the number is 0 and the id is empty.
    std::size_t number = 0;
    std::string display;
    math::Vec2 offset;
    Fill fill = Fill::None;
};

} // namespace haylen::platform
