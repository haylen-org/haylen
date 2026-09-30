#include "haylen/ai/ResponseCurve.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace haylen::ai {

const std::array<std::pair<std::string_view, ResponseCurve::Shape>, 5> ResponseCurve::kShapeNames{{{"linear", Shape::Linear}, {"polynomial", Shape::Polynomial}, {"logistic", Shape::Logistic}, {"logit", Shape::Logit}, {"normal", Shape::Normal}}};

float ResponseCurve::evaluate(float input) const noexcept {
    const float x = std::clamp(input, 0.0F, 1.0F) - shift;
    float value = 0.0F;
    switch (shape) {
    case Shape::Linear:
        value = slope * x + offset;
        break;
    case Shape::Polynomial:
        value = slope * std::pow(std::max(x, 0.0F), exponent) + offset;
        break;
    case Shape::Logistic:
        value = exponent / (1.0F + std::exp(-slope * x)) + offset;
        break;
    case Shape::Logit: {
        // The logit is infinite at both ends, so the input stays just inside them.
        const float inside = std::clamp(x, 1e-4F, 1.0F - 1e-4F);
        value = slope * std::log(inside / (1.0F - inside)) / 5.0F + 0.5F + offset;
        break;
    }
    case Shape::Normal:
        value = slope * std::exp(-exponent * x * x) + offset;
        break;
    }
    return std::clamp(value, 0.0F, 1.0F);
}

std::optional<ResponseCurve::Shape> ResponseCurve::shapeFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kShapeNames, name, &std::pair<std::string_view, Shape>::first);
    return found != kShapeNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view ResponseCurve::shapeName(Shape value) noexcept {
    return std::ranges::find(kShapeNames, value, &std::pair<std::string_view, Shape>::second)->first;
}

} // namespace haylen::ai
