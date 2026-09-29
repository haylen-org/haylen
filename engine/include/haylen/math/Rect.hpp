#pragma once

#include <algorithm>

#include "haylen/math/Insets.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::math {

struct Rect {
    float x = 0.0F;
    float y = 0.0F;
    float width = 0.0F;
    float height = 0.0F;

    [[nodiscard]] static constexpr Rect fromMinMax(Vec2 minimum, Vec2 maximum) noexcept {
        return {minimum.x, minimum.y, maximum.x - minimum.x, maximum.y - minimum.y};
    }

    [[nodiscard]] static constexpr Rect fromCenter(Vec2 middle, Vec2 extent) noexcept {
        return {middle.x - extent.x * 0.5F, middle.y - extent.y * 0.5F, extent.x, extent.y};
    }

    [[nodiscard]] constexpr bool operator==(const Rect&) const noexcept = default;

    [[nodiscard]] constexpr float getLeft() const noexcept {
        return x;
    }
    [[nodiscard]] constexpr float getRight() const noexcept {
        return x + width;
    }
    [[nodiscard]] constexpr float getTop() const noexcept {
        return y;
    }
    [[nodiscard]] constexpr float getBottom() const noexcept {
        return y + height;
    }
    [[nodiscard]] constexpr Vec2 getPosition() const noexcept {
        return {x, y};
    }
    [[nodiscard]] constexpr Vec2 getSize() const noexcept {
        return {width, height};
    }
    [[nodiscard]] constexpr Vec2 getMin() const noexcept {
        return {x, y};
    }
    [[nodiscard]] constexpr Vec2 getMax() const noexcept {
        return {getRight(), getBottom()};
    }
    [[nodiscard]] constexpr Vec2 getCenter() const noexcept {
        return {x + width * 0.5F, y + height * 0.5F};
    }
    [[nodiscard]] constexpr bool isEmpty() const noexcept {
        return width <= 0.0F || height <= 0.0F;
    }
    [[nodiscard]] constexpr float getArea() const noexcept {
        return width * height;
    }

    [[nodiscard]] constexpr bool contains(Vec2 point) const noexcept {
        return point.x >= getLeft() && point.x < getRight() && point.y >= getTop() && point.y < getBottom();
    }

    [[nodiscard]] constexpr bool contains(const Rect& other) const noexcept {
        return other.getLeft() >= getLeft() && other.getRight() <= getRight() && other.getTop() >= getTop() && other.getBottom() <= getBottom();
    }

    [[nodiscard]] constexpr bool intersects(const Rect& other) const noexcept {
        return getLeft() < other.getRight() && getRight() > other.getLeft() && getTop() < other.getBottom() && getBottom() > other.getTop();
    }

    [[nodiscard]] constexpr Rect intersection(const Rect& other) const noexcept {
        const Vec2 minimum = Vec2::max(getMin(), other.getMin());
        const Vec2 maximum = Vec2::min(getMax(), other.getMax());
        if (maximum.x <= minimum.x || maximum.y <= minimum.y) {
            return {};
        }
        return fromMinMax(minimum, maximum);
    }

    [[nodiscard]] constexpr Rect merged(const Rect& other) const noexcept {
        return fromMinMax(Vec2::min(getMin(), other.getMin()), Vec2::max(getMax(), other.getMax()));
    }

    [[nodiscard]] constexpr Rect expanded(float amount) const noexcept {
        return {x - amount, y - amount, width + amount * 2.0F, height + amount * 2.0F};
    }

    [[nodiscard]] constexpr Rect inset(const Insets& insets) const noexcept {
        return {x + insets.left, y + insets.top, std::max(0.0F, width - insets.getHorizontal()), std::max(0.0F, height - insets.getVertical())};
    }

    [[nodiscard]] constexpr Rect translated(Vec2 offset) const noexcept {
        return {x + offset.x, y + offset.y, width, height};
    }

    [[nodiscard]] constexpr Vec2 clamp(Vec2 point) const noexcept {
        return {std::clamp(point.x, getLeft(), getRight()), std::clamp(point.y, getTop(), getBottom())};
    }
};

} // namespace haylen::math
