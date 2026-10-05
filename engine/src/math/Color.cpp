#include "haylen/math/Color.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>

namespace haylen::math {

std::optional<Color> Color::parse(std::string_view text) noexcept {
    if (text.starts_with('#')) {
        text.remove_prefix(1);
    }
    if (text.size() != 6 && text.size() != 8) {
        return std::nullopt;
    }

    std::uint32_t value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, 16);
    if (error != std::errc{} || end != text.data() + text.size()) {
        return std::nullopt;
    }

    if (text.size() == 6) {
        return fromHex((value << 8U) | 0xFFU);
    }

    const std::uint32_t alpha = (value >> 24U) & 0xFFU;
    return fromHex((value << 8U) | alpha);
}

std::string Color::toHex() const {
    const std::uint32_t packed = toRgba8();
    std::array<char, 10> text{};
    std::snprintf(text.data(), text.size(), "#%02X%02X%02X%02X", (packed >> 24U) & 0xFFU, packed & 0xFFU, (packed >> 8U) & 0xFFU, (packed >> 16U) & 0xFFU);
    return text.data();
}

Color Color::fromHsv(float hue, float saturation, float value, float alpha) noexcept {
    const float wrapped = hue - std::floor(hue);
    const float sector = wrapped * 6.0F;
    const float fraction = sector - std::floor(sector);
    const float p = value * (1.0F - saturation);
    const float q = value * (1.0F - saturation * fraction);
    const float t = value * (1.0F - saturation * (1.0F - fraction));

    switch (static_cast<int>(sector) % 6) {
    case 0:
        return {value, t, p, alpha};
    case 1:
        return {q, value, p, alpha};
    case 2:
        return {p, value, t, alpha};
    case 3:
        return {p, q, value, alpha};
    case 4:
        return {t, p, value, alpha};
    default:
        return {value, p, q, alpha};
    }
}

Color::Hsv Color::toHsv() const noexcept {
    const float high = std::max({r, g, b});
    const float low = std::min({r, g, b});
    const float range = high - low;
    Hsv result{.saturation = high > 0.0F ? range / high : 0.0F, .value = high, .alpha = a};
    if (range <= 0.0F) {
        return result;
    }

    float sector = 0.0F;
    if (high == r) {
        sector = (g - b) / range;
    } else if (high == g) {
        sector = 2.0F + (b - r) / range;
    } else {
        sector = 4.0F + (r - g) / range;
    }
    result.hue = sector / 6.0F - std::floor(sector / 6.0F);
    return result;
}

Color Color::lerpHsv(const Color& from, const Color& to, float t) noexcept {
    Hsv start = from.toHsv();
    Hsv end = to.toHsv();

    // A gray has no hue of its own, so it takes the hue of the other color and only its saturation changes.
    if (start.saturation <= 0.0F) {
        start.hue = end.hue;
    }
    if (end.saturation <= 0.0F) {
        end.hue = start.hue;
    }

    float hueDelta = end.hue - start.hue;
    hueDelta -= std::round(hueDelta);
    return fromHsv(start.hue + hueDelta * t, start.saturation + (end.saturation - start.saturation) * t, start.value + (end.value - start.value) * t, start.alpha + (end.alpha - start.alpha) * t);
}

} // namespace haylen::math
