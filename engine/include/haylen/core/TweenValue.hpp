#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <variant>

#include "haylen/math/Color.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::core {

// A value that a tween animates: a number, a Vec2, a Color or a text, with the arithmetic that tweens need to travel between two values of the same kind.
class TweenValue final {
  public:
    enum class Kind : std::uint8_t {
        Number,
        Vector,
        Color,
        Text,
    };

    // How a value travels from its start to its end. Angle takes the shorter way around the circle in radians, Integer rounds numbers such as score counters, and Hsv blends colors through hue, saturation and value.
    enum class Interpolation : std::uint8_t {
        Linear,
        Angle,
        Integer,
        Hsv,
    };

    TweenValue() = default;
    TweenValue(double value) : data(value) {}
    TweenValue(math::Vec2 value) : data(value) {}
    TweenValue(math::Color value) : data(value) {}
    TweenValue(std::string value) : data(std::move(value)) {}

    [[nodiscard]] Kind getKind() const noexcept {
        return static_cast<Kind>(data.index());
    }
    [[nodiscard]] double getNumber() const {
        return std::get<double>(data);
    }
    [[nodiscard]] math::Vec2 getVector() const {
        return std::get<math::Vec2>(data);
    }
    [[nodiscard]] math::Color getColor() const {
        return std::get<math::Color>(data);
    }
    [[nodiscard]] const std::string& getText() const {
        return std::get<std::string>(data);
    }

    // Both values must have the same kind. A text reveals the end text over the start text one character at a time, like a typewriter.
    [[nodiscard]] static TweenValue mix(const TweenValue& from, const TweenValue& to, float t, Interpolation interpolation);

    // Returns value plus times the offset, which is how relative tweens and incremental loops move their range. Texts cannot be offset.
    [[nodiscard]] static TweenValue add(const TweenValue& value, const TweenValue& offset, float times = 1.0F);

    // Returns the offset from start to end, the inverse of add.
    [[nodiscard]] static TweenValue difference(const TweenValue& end, const TweenValue& start);

    // Measures how far apart the values are, which is how speed-based tweens find their duration: units for numbers and vectors, the largest channel change for colors and characters for texts.
    [[nodiscard]] static float distance(const TweenValue& from, const TweenValue& to, Interpolation interpolation);

    [[nodiscard]] bool operator==(const TweenValue&) const = default;

  private:
    [[nodiscard]] static std::string mixText(const std::string& from, const std::string& to, float t);
    static void requireSameKind(const TweenValue& first, const TweenValue& second);

    std::variant<double, math::Vec2, math::Color, std::string> data;
};

} // namespace haylen::core
