#include "haylen/text/TextEffect.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <utility>

#include <fast_float/fast_float.h>

#include "haylen/math/Math.hpp"
#include "text/MarkupParser.hpp"

namespace haylen::text {

TextEffect::Parameters::Parameters(std::string effectName, std::map<std::string, std::string, std::less<>> attributes) : effect(std::move(effectName)), values(std::move(attributes)) {}

bool TextEffect::Parameters::has(std::string_view key) const {
    return values.contains(key);
}

float TextEffect::Parameters::getNumber(std::string_view key, float byDefault) const {
    const auto found = values.find(key);
    if (found == values.end()) {
        return byDefault;
    }
    const std::string& text = found->second;
    float value = 0.0F;
    const auto [end, error] = fast_float::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size() || text.empty()) {
        throw std::invalid_argument("The " + std::string(key) + " of [" + effect + "] must be a number, not " + text + ".");
    }
    return value;
}

math::Color TextEffect::Parameters::getColor(std::string_view key, math::Color byDefault) const {
    const auto found = values.find(key);
    if (found == values.end()) {
        return byDefault;
    }
    const std::optional<math::Color> parsed = MarkupParser::parseColor(found->second);
    if (!parsed) {
        throw std::invalid_argument("The " + std::string(key) + " of [" + effect + "] must be a color such as red or #RRGGBB, not " + found->second + ".");
    }
    return *parsed;
}

// The easing curve of Godot, where a curve above 1 eases in, between 0 and 1 eases out and below 0 eases in and out.
float TextEffect::ease(float value, float curve) noexcept {
    const float x = std::clamp(value, 0.0F, 1.0F);
    if (curve > 0.0F) {
        return curve < 1.0F ? 1.0F - std::pow(1.0F - x, 1.0F / curve) : std::pow(x, curve);
    }
    if (curve < 0.0F) {
        return x < 0.5F ? std::pow(x * 2.0F, -curve) * 0.5F : (1.0F - std::pow(1.0F - (x - 0.5F) * 2.0F, -curve)) * 0.5F + 0.5F;
    }
    return 0.0F;
}

float TextEffect::pingPong(float value, float length) noexcept {
    if (length == 0.0F) {
        return 0.0F;
    }
    const float cycle = (value - length) / (length * 2.0F);
    return std::fabs((cycle - std::floor(cycle)) * length * 2.0F - length);
}

// A stable pseudo-random angle for a character at a step of the shake, so the same time always shakes the same way.
float TextEffect::randomAngle(std::size_t character, std::int64_t step) noexcept {
    std::uint64_t hash = static_cast<std::uint64_t>(character) * 0x9E3779B97F4A7C15ULL ^ static_cast<std::uint64_t>(step) * 0xC2B2AE3D27D4EB4FULL;
    hash ^= hash >> 31U;
    hash *= 0xBF58476D1CE4E5B9ULL;
    hash ^= hash >> 27U;
    return static_cast<float>(hash >> 40U) / static_cast<float>(1U << 24U) * math::Math::kTau;
}

void TextEffect::wave(Glyph& glyph, const Parameters& parameters) {
    const float amplitude = parameters.getNumber("amp", 20.0F);
    const float frequency = parameters.getNumber("freq", 5.0F);
    glyph.offset.y += std::sin(frequency * glyph.time + glyph.position.x / kPhaseSpan) * amplitude / 10.0F;
}

// Each character jumps to a new random offset rate times a second, easing there over the first half of the step.
void TextEffect::shake(Glyph& glyph, const Parameters& parameters) {
    const float rate = std::max(parameters.getNumber("rate", 20.0F), 0.001F);
    const float level = parameters.getNumber("level", 5.0F);
    const float steps = glyph.time * rate;
    const auto step = static_cast<std::int64_t>(std::floor(steps));
    const float blend = std::min((steps - std::floor(steps)) * 2.0F, 1.0F);
    const float previous = randomAngle(glyph.character, step - 1);
    const float current = randomAngle(glyph.character, step);
    const math::Vec2 from{std::sin(previous), std::cos(previous)};
    const math::Vec2 to{std::sin(current), std::cos(current)};
    glyph.offset += (from + (to - from) * blend) * (level / 10.0F);
}

void TextEffect::tornado(Glyph& glyph, const Parameters& parameters) {
    const float radius = parameters.getNumber("radius", 10.0F);
    const float frequency = parameters.getNumber("freq", 1.0F);
    const float phase = frequency * glyph.time + glyph.position.x / kPhaseSpan;
    glyph.offset += math::Vec2{std::sin(phase), std::cos(phase)} * radius;
}

void TextEffect::fade(Glyph& glyph, const Parameters& parameters) {
    const float start = parameters.getNumber("start", 0.0F);
    const float length = std::max(parameters.getNumber("length", 10.0F), 1.0F);
    const auto index = static_cast<float>(glyph.index);
    if (index >= start) {
        glyph.color.a *= std::max(1.0F - (index - start) / length, 0.0F);
    }
}

void TextEffect::rainbow(Glyph& glyph, const Parameters& parameters) {
    const float frequency = std::max(parameters.getNumber("freq", 1.0F), 0.0F);
    const float saturation = parameters.getNumber("sat", 0.8F);
    const float value = parameters.getNumber("val", 0.8F);
    const float speed = parameters.getNumber("speed", 1.0F);
    glyph.color = math::Color::fromHsv(frequency * std::fabs(glyph.time * speed + glyph.position.x / kPhaseSpan), saturation, value, glyph.color.a);
}

void TextEffect::pulse(Glyph& glyph, const Parameters& parameters) {
    const float frequency = std::max(parameters.getNumber("freq", 1.0F), 0.001F);
    const math::Color tint = parameters.getColor("color", math::Color{1.0F, 1.0F, 1.0F, 0.25F});
    const float amount = ease(pingPong(glyph.time, 1.0F / frequency) * frequency, parameters.getNumber("ease", -2.0F));
    glyph.color = math::Color::lerp(glyph.color, glyph.color * tint, amount);
}

} // namespace haylen::text
