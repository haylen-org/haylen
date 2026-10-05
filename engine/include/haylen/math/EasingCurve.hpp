#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <vector>

#include "haylen/math/Easing.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::math {

// A curve that maps progress from 0 to 1 onto eased progress: a standard curve, a back or elastic curve with its own parameters, steps, a CSS cubic Bézier, a polyline through points or any function. Tweens and scene transitions ease with it. The default curve is linear.
class EasingCurve final {
  public:
    // The form of the curve, which bindings read back to describe it the way they read it.
    enum class Kind : std::uint8_t {
        Preset,
        Parametric,
        Steps,
        CubicBezier,
        Points,
        Custom,
    };

    EasingCurve() noexcept = default;
    explicit EasingCurve(Easing::Type curve) noexcept;

    [[nodiscard]] static EasingCurve back(Easing::Type curve, float overshoot);
    [[nodiscard]] static EasingCurve elastic(Easing::Type curve, float amplitude, float period);
    [[nodiscard]] static EasingCurve steps(int count, Easing::StepPosition position = Easing::StepPosition::End);
    [[nodiscard]] static EasingCurve cubicBezier(float x1, float y1, float x2, float y2);

    // Joins the points with straight lines. Their x coordinates must grow from the first point to the last, and progress outside them takes the value of the nearest end.
    [[nodiscard]] static EasingCurve points(std::vector<Vec2> values);

    // Uses the function, which receives progress in [0, 1], as the curve.
    [[nodiscard]] static EasingCurve custom(std::function<float(float)> function);

    [[nodiscard]] float apply(float t) const;

    // Tells whether the curve returns its progress unchanged, so callers can skip it.
    [[nodiscard]] bool isLinear() const noexcept {
        return kind == Kind::Preset && type == Easing::Type::Linear;
    }
    [[nodiscard]] Kind getKind() const noexcept {
        return kind;
    }

    // The standard curve of preset and parametric curves.
    [[nodiscard]] Easing::Type getType() const noexcept {
        return type;
    }
    [[nodiscard]] Easing::StepPosition getStepPosition() const noexcept {
        return stepPosition;
    }

    // The overshoot of back curves, the amplitude and period of elastic curves, the count of steps curves and the four handles of cubic Bézier curves, in that order.
    [[nodiscard]] const std::array<float, 4>& getParameters() const noexcept {
        return parameters;
    }
    [[nodiscard]] std::span<const Vec2> getPoints() const noexcept {
        return polyline ? std::span<const Vec2>(*polyline) : std::span<const Vec2>();
    }

  private:
    [[nodiscard]] float applyPoints(float t) const noexcept;

    Kind kind = Kind::Preset;
    Easing::Type type = Easing::Type::Linear;
    Easing::StepPosition stepPosition = Easing::StepPosition::End;
    std::array<float, 4> parameters{};
    std::shared_ptr<const std::vector<Vec2>> polyline;
    std::function<float(float)> callback;
};

} // namespace haylen::math
