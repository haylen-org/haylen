#include "haylen/ai/ResponseCurve.hpp"

#include <algorithm>
#include <cmath>

namespace haylen::ai {

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
    if (name == "linear") {
        return Shape::Linear;
    }
    if (name == "polynomial") {
        return Shape::Polynomial;
    }
    if (name == "logistic") {
        return Shape::Logistic;
    }
    if (name == "logit") {
        return Shape::Logit;
    }
    if (name == "normal") {
        return Shape::Normal;
    }
    return std::nullopt;
}

std::string_view ResponseCurve::shapeName(Shape value) noexcept {
    switch (value) {
    case Shape::Polynomial:
        return "polynomial";
    case Shape::Logistic:
        return "logistic";
    case Shape::Logit:
        return "logit";
    case Shape::Normal:
        return "normal";
    case Shape::Linear:
        break;
    }
    return "linear";
}

} // namespace haylen::ai
