#include "haylen/math/Color.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>

#include "haylen/math/ColorSpace.hpp"
#include "haylen/math/Vec3.hpp"

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

    // A hue that is not finite has no sector, so the channels of its color are not numbers either.
    if (!std::isfinite(sector)) {
        const float unknown = std::numeric_limits<float>::quiet_NaN();
        return {unknown, unknown, unknown, alpha};
    }

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

Color Color::getLinear() const noexcept {
    return {ColorSpace::decodeSrgb(r), ColorSpace::decodeSrgb(g), ColorSpace::decodeSrgb(b), a};
}

Color Color::fromLinear(const Color& linear) noexcept {
    return {ColorSpace::encodeSrgb(linear.r), ColorSpace::encodeSrgb(linear.g), ColorSpace::encodeSrgb(linear.b), linear.a};
}

// The chromaticity of the light of a black body follows the cubic fits of Kim and others to the Planckian locus, which hold from 1667 to 25000 kelvin.
Color Color::fromTemperature(float kelvin) {
    if (!(kelvin >= 1667.0F && kelvin <= 25000.0F)) {
        throw std::invalid_argument("A color temperature must be from 1667 to 25000 kelvin.");
    }

    const double t = kelvin;
    const double x = kelvin <= 4000.0F ? -0.2661239e9 / (t * t * t) - 0.2343589e6 / (t * t) + 0.8776956e3 / t + 0.179910 : -3.0258469e9 / (t * t * t) + 2.1070379e6 / (t * t) + 0.2226347e3 / t + 0.240390;
    double y = 3.0817580 * x * x * x - 5.87338670 * x * x + 3.75112997 * x - 0.37001483;
    if (kelvin <= 2222.0F) {
        y = -1.1063814 * x * x * x - 1.34811020 * x * x + 2.18555832 * x - 0.20219683;
    } else if (kelvin <= 4000.0F) {
        y = -0.9549476 * x * x * x - 1.37418593 * x * x + 2.09137015 * x - 0.16748867;
    }

    const Vec3 xyz{static_cast<float>(x / y), 1.0F, static_cast<float>((1.0 - x - y) / y)};
    const Color light = ColorSpace::fromXyz(xyz, ColorSpace::Kind::Srgb);
    const Color inGamut{std::max(light.r, 0.0F), std::max(light.g, 0.0F), std::max(light.b, 0.0F), 1.0F};
    const float largest = std::max({inGamut.r, inGamut.g, inGamut.b});
    return fromLinear({inGamut.r / largest, inGamut.g / largest, inGamut.b / largest, 1.0F});
}

float Color::getLuminance() const noexcept {
    return ColorSpace::luminance(getLinear(), ColorSpace::Kind::Srgb);
}

float Color::contrast(const Color& first, const Color& second) noexcept {
    const float lighter = std::max(first.getLuminance(), second.getLuminance());
    const float darker = std::min(first.getLuminance(), second.getLuminance());
    return (lighter + 0.05F) / (darker + 0.05F);
}

// White reads better than black exactly where the contrast with white is higher, which is below the luminance whose contrast with both is the same.
bool Color::isDark() const noexcept {
    return contrast(*this, white()) > contrast(*this, black());
}

Color Color::getContrastingColor() const noexcept {
    return (isDark() ? white() : black()).withAlpha(a);
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
