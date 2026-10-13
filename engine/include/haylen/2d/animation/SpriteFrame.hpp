#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::animation2d {

// One frame of a sprite sheet. Trimmed atlas frames remember where the trimmed rectangle sits inside the original frame, so pivots stay stable while playing. A frame lasts its duration in seconds, or its ticks when an animation counts fixed steps, and carries the data a game reads at it: a pivot, named boxes such as hitboxes, named points such as a hand or a muzzle, and named events. Pivots, boxes and points are in pixels of the untrimmed frame.
struct SpriteFrame {
    struct Box {
        std::string name;
        math::Rect area{};
    };

    struct Point {
        std::string name;
        math::Vec2 position{};
    };

    math::Rect source{};
    math::Vec2 offset{};
    math::Vec2 originalSize{};
    float duration = 0.1F;
    std::uint32_t ticks = 0;
    std::optional<math::Vec2> pivot;
    std::vector<Box> boxes;
    std::vector<Point> points;
    std::vector<std::string> events;

    [[nodiscard]] const Box* findBox(std::string_view name) const noexcept {
        for (const Box& box : boxes) {
            if (box.name == name) {
                return &box;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const Point* findPoint(std::string_view name) const noexcept {
        for (const Point& point : points) {
            if (point.name == name) {
                return &point;
            }
        }
        return nullptr;
    }
};

} // namespace haylen::animation2d
