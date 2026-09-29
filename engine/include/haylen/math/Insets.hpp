#pragma once

namespace haylen::math {

struct Insets {
    float left = 0.0F;
    float top = 0.0F;
    float right = 0.0F;
    float bottom = 0.0F;

    [[nodiscard]] static constexpr Insets uniform(float value) noexcept {
        return {value, value, value, value};
    }
    [[nodiscard]] constexpr bool operator==(const Insets&) const noexcept = default;
    [[nodiscard]] constexpr float getHorizontal() const noexcept {
        return left + right;
    }
    [[nodiscard]] constexpr float getVertical() const noexcept {
        return top + bottom;
    }
};

} // namespace haylen::math
