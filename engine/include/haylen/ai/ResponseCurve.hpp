#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace haylen::ai {

// Turns an input from 0 to 1 into a utility from 0 to 1, with the curve shapes of the infinite axis utility system. Inputs and results are clamped to that range.
struct ResponseCurve {
    // With x the input, linear gives slope * (x - shift) + offset and ignores the exponent, polynomial gives slope * (x - shift) ^ exponent + offset, logistic gives exponent / (1 + e ^ (-slope * (x - shift))) + offset, logit gives slope * ln((x - shift) / (1 - x + shift)) / 5 + 0.5 + offset, and normal gives slope * e ^ (-exponent * (x - shift) ^ 2) + offset. Polynomials treat inputs below the shift as the shift.
    enum class Shape : std::uint8_t {
        Linear,
        Polynomial,
        Logistic,
        Logit,
        Normal,
    };

    Shape shape = Shape::Linear;
    float slope = 1.0F;
    float exponent = 1.0F;
    float shift = 0.0F;
    float offset = 0.0F;

    [[nodiscard]] float evaluate(float input) const noexcept;

    [[nodiscard]] static std::optional<Shape> shapeFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view shapeName(Shape value) noexcept;
};

} // namespace haylen::ai
