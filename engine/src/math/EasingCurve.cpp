#include "haylen/math/EasingCurve.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "haylen/math/Math.hpp"

namespace haylen::math {

EasingCurve::EasingCurve(Easing::Type curve) noexcept : type(curve) {}

EasingCurve EasingCurve::back(Easing::Type curve, float overshoot) {
    if (curve != Easing::Type::BackIn && curve != Easing::Type::BackOut && curve != Easing::Type::BackInOut) {
        throw std::invalid_argument("An overshoot only applies to the back curves.");
    }
    EasingCurve result(curve);
    result.kind = Kind::Parametric;
    result.parameters = {overshoot, 0.0F, 0.0F, 0.0F};
    return result;
}

EasingCurve EasingCurve::elastic(Easing::Type curve, float amplitude, float period) {
    if (curve != Easing::Type::ElasticIn && curve != Easing::Type::ElasticOut && curve != Easing::Type::ElasticInOut) {
        throw std::invalid_argument("An amplitude and a period only apply to the elastic curves.");
    }
    if (period <= 0.0F) {
        throw std::invalid_argument("An elastic curve needs a positive period.");
    }
    EasingCurve result(curve);
    result.kind = Kind::Parametric;
    result.parameters = {amplitude, period, 0.0F, 0.0F};
    return result;
}

EasingCurve EasingCurve::steps(int count, Easing::StepPosition position) {
    if (count < 1) {
        throw std::invalid_argument("A steps curve needs at least one step.");
    }
    EasingCurve result;
    result.kind = Kind::Steps;
    result.stepPosition = position;
    result.parameters = {static_cast<float>(count), 0.0F, 0.0F, 0.0F};
    return result;
}

EasingCurve EasingCurve::cubicBezier(float x1, float y1, float x2, float y2) {
    if (x1 < 0.0F || x1 > 1.0F || x2 < 0.0F || x2 > 1.0F) {
        throw std::invalid_argument("The x coordinates of a cubic Bézier curve must be between 0 and 1.");
    }
    EasingCurve result;
    result.kind = Kind::CubicBezier;
    result.parameters = {x1, y1, x2, y2};
    return result;
}

EasingCurve EasingCurve::points(std::vector<Vec2> values) {
    if (values.size() < 2) {
        throw std::invalid_argument("A points curve needs at least two points.");
    }
    if (!std::is_sorted(values.begin(), values.end(), [](Vec2 lhs, Vec2 rhs) { return lhs.x < rhs.x; })) {
        throw std::invalid_argument("The points of a curve must be ordered by x.");
    }
    EasingCurve result;
    result.kind = Kind::Points;
    result.polyline = std::make_shared<const std::vector<Vec2>>(std::move(values));
    return result;
}

EasingCurve EasingCurve::custom(std::function<float(float)> function) {
    if (!function) {
        throw std::invalid_argument("A custom curve needs a function.");
    }
    EasingCurve result;
    result.kind = Kind::Custom;
    result.callback = std::move(function);
    return result;
}

float EasingCurve::applyPoints(float t) const noexcept {
    // The negated comparisons send NaN to the first point, so the search below always finds a point after `t`.
    const std::vector<Vec2>& line = *polyline;
    if (!(t > line.front().x)) {
        return line.front().y;
    }
    if (!(t < line.back().x)) {
        return line.back().y;
    }

    const auto after = std::upper_bound(line.begin(), line.end(), t, [](float value, Vec2 point) { return value < point.x; });
    const Vec2 end = *after;
    const Vec2 start = *(after - 1);
    const float span = end.x - start.x;
    return span <= 0.0F ? end.y : Math::lerp(start.y, end.y, (t - start.x) / span);
}

float EasingCurve::apply(float t) const {
    switch (kind) {
    case Kind::Preset:
        return Easing::apply(type, t);
    case Kind::Parametric:
        return Easing::apply(type, t, parameters[0], parameters[1]);
    case Kind::Steps:
        return Easing::steps(t, static_cast<int>(parameters[0]), stepPosition);
    case Kind::CubicBezier:
        return Easing::cubicBezier(t, parameters[0], parameters[1], parameters[2], parameters[3]);
    case Kind::Points:
        return applyPoints(Math::saturate(t));
    case Kind::Custom:
        return callback(Math::saturate(t));
    }
    return t;
}

} // namespace haylen::math
