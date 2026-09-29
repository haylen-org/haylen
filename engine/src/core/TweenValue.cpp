#include "haylen/core/TweenValue.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "haylen/core/Utf8.hpp"
#include "haylen/math/Math.hpp"

namespace haylen::core {

void TweenValue::requireSameKind(const TweenValue& first, const TweenValue& second) {
    if (first.getKind() != second.getKind()) {
        throw std::invalid_argument("A tween can only travel between two values of the same kind.");
    }
}

std::string TweenValue::mixText(const std::string& from, const std::string& to, float t) {
    const std::size_t total = Utf8::countCodePoints(to);
    const auto revealed = static_cast<std::size_t>(std::lround(std::clamp(t, 0.0F, 1.0F) * static_cast<float>(total)));

    std::size_t toOffset = 0;
    for (std::size_t index = 0; index < revealed; ++index) {
        (void)Utf8::decode(to, toOffset);
    }

    // The rest of the start text stays visible behind the revealed part, so a text can type over another one.
    std::size_t fromOffset = 0;
    for (std::size_t index = 0; index < revealed && fromOffset < from.size(); ++index) {
        (void)Utf8::decode(from, fromOffset);
    }
    std::string result = to.substr(0, toOffset);
    result.append(from, std::min(fromOffset, from.size()));
    return result;
}

TweenValue TweenValue::mix(const TweenValue& from, const TweenValue& to, float t, Interpolation interpolation) {
    requireSameKind(from, to);
    switch (from.getKind()) {
    case Kind::Number: {
        const double start = from.getNumber();
        double end = to.getNumber();
        if (interpolation == Interpolation::Angle) {
            end = start + static_cast<double>(math::Math::wrapAngle(static_cast<float>(end - start)));
        }
        const double value = start * (1.0 - static_cast<double>(t)) + end * static_cast<double>(t);
        return interpolation == Interpolation::Integer ? std::round(value) : value;
    }
    case Kind::Vector:
        return math::Vec2::lerp(from.getVector(), to.getVector(), t);
    case Kind::Color:
        return interpolation == Interpolation::Hsv ? math::Color::lerpHsv(from.getColor(), to.getColor(), t) : math::Color::lerp(from.getColor(), to.getColor(), t);
    case Kind::Text:
        return mixText(from.getText(), to.getText(), t);
    }
    return from;
}

TweenValue TweenValue::add(const TweenValue& value, const TweenValue& offset, float times) {
    requireSameKind(value, offset);
    switch (value.getKind()) {
    case Kind::Number:
        return value.getNumber() + offset.getNumber() * static_cast<double>(times);
    case Kind::Vector:
        return value.getVector() + offset.getVector() * times;
    case Kind::Color: {
        const math::Color base = value.getColor();
        const math::Color delta = offset.getColor();
        return math::Color{base.r + delta.r * times, base.g + delta.g * times, base.b + delta.b * times, base.a + delta.a * times};
    }
    case Kind::Text:
        break;
    }
    throw std::invalid_argument("A text cannot be tweened by an offset.");
}

TweenValue TweenValue::difference(const TweenValue& end, const TweenValue& start) {
    requireSameKind(end, start);
    switch (end.getKind()) {
    case Kind::Number:
        return end.getNumber() - start.getNumber();
    case Kind::Vector:
        return end.getVector() - start.getVector();
    case Kind::Color: {
        const math::Color last = end.getColor();
        const math::Color first = start.getColor();
        return math::Color{last.r - first.r, last.g - first.g, last.b - first.b, last.a - first.a};
    }
    case Kind::Text:
        break;
    }
    throw std::invalid_argument("A text cannot be tweened by an offset.");
}

float TweenValue::distance(const TweenValue& from, const TweenValue& to, Interpolation interpolation) {
    requireSameKind(from, to);
    switch (from.getKind()) {
    case Kind::Number: {
        const auto delta = static_cast<float>(to.getNumber() - from.getNumber());
        return std::fabs(interpolation == Interpolation::Angle ? math::Math::wrapAngle(delta) : delta);
    }
    case Kind::Vector:
        return math::Vec2::distance(from.getVector(), to.getVector());
    case Kind::Color: {
        const math::Color first = from.getColor();
        const math::Color last = to.getColor();
        return std::max({std::fabs(last.r - first.r), std::fabs(last.g - first.g), std::fabs(last.b - first.b), std::fabs(last.a - first.a)});
    }
    case Kind::Text:
        return static_cast<float>(Utf8::countCodePoints(to.getText()));
    }
    return 0.0F;
}

} // namespace haylen::core
