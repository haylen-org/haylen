#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace haylen::math {

struct Color {
    // Hue, saturation and value of a color, with the hue in turns like fromHsv.
    struct Hsv {
        float hue = 0.0F;
        float saturation = 0.0F;
        float value = 0.0F;
        float alpha = 1.0F;
    };

    float r = 1.0F;
    float g = 1.0F;
    float b = 1.0F;
    float a = 1.0F;

    [[nodiscard]] static constexpr Color white() noexcept {
        return {1.0F, 1.0F, 1.0F, 1.0F};
    }
    [[nodiscard]] static constexpr Color black() noexcept {
        return {0.0F, 0.0F, 0.0F, 1.0F};
    }
    [[nodiscard]] static constexpr Color transparent() noexcept {
        return {0.0F, 0.0F, 0.0F, 0.0F};
    }

    [[nodiscard]] static constexpr Color fromRgba8(std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha = 255) noexcept {
        return {static_cast<float>(red) / 255.0F, static_cast<float>(green) / 255.0F, static_cast<float>(blue) / 255.0F, static_cast<float>(alpha) / 255.0F};
    }

    [[nodiscard]] static constexpr Color fromHex(std::uint32_t rrggbbaa) noexcept {
        return fromRgba8(static_cast<std::uint8_t>((rrggbbaa >> 24U) & 0xFFU), static_cast<std::uint8_t>((rrggbbaa >> 16U) & 0xFFU), static_cast<std::uint8_t>((rrggbbaa >> 8U) & 0xFFU), static_cast<std::uint8_t>(rrggbbaa & 0xFFU));
    }

    // Parses "#RRGGBB" and "#AARRGGBB", the notations used by Tiled, with or without the hash.
    [[nodiscard]] static std::optional<Color> parse(std::string_view text) noexcept;
    // Hue is measured in turns, so 1/3 is green and values outside 0 to 1 wrap around.
    [[nodiscard]] static Color fromHsv(float hue, float saturation, float value, float alpha = 1.0F) noexcept;

    [[nodiscard]] static constexpr Color lerp(const Color& from, const Color& to, float t) noexcept {
        const float keep = 1.0F - t;
        return {from.r * keep + to.r * t, from.g * keep + to.g * t, from.b * keep + to.b * t, from.a * keep + to.a * t};
    }

    // Interpolates hue, saturation, value and alpha, taking the shorter way around the hue circle, so red to blue passes through magenta instead of gray.
    [[nodiscard]] static Color lerpHsv(const Color& from, const Color& to, float t) noexcept;

    [[nodiscard]] Hsv toHsv() const noexcept;

    [[nodiscard]] constexpr bool operator==(const Color&) const noexcept = default;

    [[nodiscard]] constexpr Color operator*(const Color& other) const noexcept {
        return {r * other.r, g * other.g, b * other.b, a * other.a};
    }

    [[nodiscard]] constexpr Color withAlpha(float alpha) const noexcept {
        return {r, g, b, alpha};
    }
    [[nodiscard]] constexpr Color getPremultiplied() const noexcept {
        return {r * a, g * a, b * a, a};
    }

    // Packs the color as RGBA8 bytes in memory order, the layout of UBYTE4N vertex attributes.
    [[nodiscard]] std::uint32_t toRgba8() const noexcept;

    // Formats the color as "#AARRGGBB", which parse reads back.
    [[nodiscard]] std::string toHex() const;
};

} // namespace haylen::math
