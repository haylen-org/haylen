#pragma once

#include <array>
#include <cmath>
#include <span>
#include <string_view>

namespace haylen::math {

// Four floats that the batch math computes one at a time, the reference that the vector instructions of `Float4` must match and the lanes of a processor without them.
class ScalarFloat4 final {
  public:
    using Value = std::array<float, 4>;

    static constexpr std::string_view kInstructions = "scalar";

    [[nodiscard]] static Value load(std::span<const float, 4> values) noexcept {
        return {values[0], values[1], values[2], values[3]};
    }

    static void store(const Value& value, std::span<float, 4> values) noexcept {
        values[0] = value[0];
        values[1] = value[1];
        values[2] = value[2];
        values[3] = value[3];
    }

    [[nodiscard]] static Value splat(float value) noexcept {
        return {value, value, value, value};
    }

    [[nodiscard]] static Value add(const Value& lhs, const Value& rhs) noexcept {
        return {lhs[0] + rhs[0], lhs[1] + rhs[1], lhs[2] + rhs[2], lhs[3] + rhs[3]};
    }

    [[nodiscard]] static Value subtract(const Value& lhs, const Value& rhs) noexcept {
        return {lhs[0] - rhs[0], lhs[1] - rhs[1], lhs[2] - rhs[2], lhs[3] - rhs[3]};
    }

    [[nodiscard]] static Value multiply(const Value& lhs, const Value& rhs) noexcept {
        return {lhs[0] * rhs[0], lhs[1] * rhs[1], lhs[2] * rhs[2], lhs[3] * rhs[3]};
    }

    [[nodiscard]] static Value absolute(const Value& value) noexcept {
        return {std::fabs(value[0]), std::fabs(value[1]), std::fabs(value[2]), std::fabs(value[3])};
    }

    // Returns whether any lane of `lhs` lies below the same lane of `rhs`.
    [[nodiscard]] static bool anyLess(const Value& lhs, const Value& rhs) noexcept {
        return lhs[0] < rhs[0] || lhs[1] < rhs[1] || lhs[2] < rhs[2] || lhs[3] < rhs[3];
    }
};

} // namespace haylen::math
