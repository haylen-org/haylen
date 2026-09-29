#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace haylen::math {

// The standard easing curves, which map progress from 0 to 1 onto an eased 0 to 1.
class Easing final {
  public:
    enum class Type : std::uint8_t {
        Linear,
        SineIn,
        SineOut,
        SineInOut,
        QuadIn,
        QuadOut,
        QuadInOut,
        CubicIn,
        CubicOut,
        CubicInOut,
        QuartIn,
        QuartOut,
        QuartInOut,
        QuintIn,
        QuintOut,
        QuintInOut,
        ExpoIn,
        ExpoOut,
        ExpoInOut,
        CircIn,
        CircOut,
        CircInOut,
        BackIn,
        BackOut,
        BackInOut,
        ElasticIn,
        ElasticOut,
        ElasticInOut,
        BounceIn,
        BounceOut,
        BounceInOut,
    };

    // Where the jumps of a steps curve fall, like the jump terms of CSS steps(): Start jumps at the start of each step, End at its end, Both at both ends of the range and None at neither.
    enum class StepPosition : std::uint8_t {
        Start,
        End,
        Both,
        None,
    };

    static constexpr float kBackOvershoot = 1.70158F;
    static constexpr float kElasticAmplitude = 1.0F;
    static constexpr float kElasticPeriod = 0.3F;

    // Evaluates the easing curve at t, which is clamped to [0, 1].
    [[nodiscard]] static float apply(Type curve, float t) noexcept;

    // Evaluates a back curve with its overshoot, or an elastic curve with its amplitude and period, at t clamped to [0, 1]. Any other curve ignores the parameters.
    [[nodiscard]] static float apply(Type curve, float t, float first, float second) noexcept;

    // Evaluates count equal steps from 0 to 1 at t clamped to [0, 1].
    [[nodiscard]] static float steps(float t, int count, StepPosition position) noexcept;

    // Evaluates the CSS cubic Bézier curve through (0, 0), (x1, y1), (x2, y2) and (1, 1) at t clamped to [0, 1]. The x coordinates are clamped to [0, 1] like CSS requires.
    [[nodiscard]] static float cubicBezier(float t, float x1, float y1, float x2, float y2) noexcept;

    // Resolves names such as "linear", "quad_out" or "elastic_in_out".
    [[nodiscard]] static std::optional<Type> parse(std::string_view text) noexcept;
    [[nodiscard]] static std::string_view name(Type curve) noexcept;

    [[nodiscard]] static std::optional<StepPosition> parseStepPosition(std::string_view text) noexcept;
    [[nodiscard]] static std::string_view stepPositionName(StepPosition position) noexcept;

  private:
    using Curve = float (*)(float) noexcept;

    // Shapes an in curve as the in, out or in-out variant that the type names.
    template <typename In> [[nodiscard]] static float shape(Type curve, float t, In in) noexcept {
        const int variant = (static_cast<int>(curve) - 1) % 3;
        if (variant == 0) {
            return in(t);
        }
        if (variant == 1) {
            return 1.0F - in(1.0F - t);
        }
        return t < 0.5F ? in(t * 2.0F) * 0.5F : 1.0F - in((1.0F - t) * 2.0F) * 0.5F;
    }

    [[nodiscard]] static float backIn(float t, float overshoot) noexcept;
    [[nodiscard]] static float elasticIn(float t, float amplitude, float period) noexcept;
    [[nodiscard]] static float bezierComponent(float u, float first, float second) noexcept;
    [[nodiscard]] static float bezierSlope(float u, float first, float second) noexcept;

    [[nodiscard]] static float out(float t, Curve curve) noexcept;
    [[nodiscard]] static float inOut(float t, Curve curve) noexcept;
    [[nodiscard]] static float power(float t, int exponent) noexcept;
    [[nodiscard]] static float sineIn(float t) noexcept;
    [[nodiscard]] static float quadIn(float t) noexcept;
    [[nodiscard]] static float cubicIn(float t) noexcept;
    [[nodiscard]] static float quartIn(float t) noexcept;
    [[nodiscard]] static float quintIn(float t) noexcept;
    [[nodiscard]] static float expoIn(float t) noexcept;
    [[nodiscard]] static float circIn(float t) noexcept;
    [[nodiscard]] static float bounceIn(float t) noexcept;
    [[nodiscard]] static float bounceOut(float t) noexcept;

    static const std::array<std::pair<std::string_view, Type>, 31> kNames;
    static const std::array<std::pair<std::string_view, StepPosition>, 4> kStepPositions;
};

} // namespace haylen::math
