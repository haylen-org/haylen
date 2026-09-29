#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>

#include "haylen/math/Color.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::text {

// An animation that rich text runs on every glyph inside its tag each frame, such as [wave amp=20 freq=5]. The built-in effects are wave, shake, tornado, fade, rainbow and pulse.
class TextEffect final {
  public:
    // What an effect sees of one glyph, and the offset, color and visibility it may change. The index counts the characters inside the tag and the character counts them in the whole text. The position is the pen position on the baseline, and the time is the seconds the text has run.
    struct Glyph {
        std::size_t index = 0;
        std::size_t character = 0;
        char32_t codePoint = 0;
        math::Vec2 position{};
        float time = 0.0F;
        math::Vec2 offset{};
        math::Color color = math::Color::white();
        bool visible = true;
    };

    // The attributes of an effect tag, where a value given as [name=value] is the attribute named value. Numbers and colors are read once, when the markup is set, rather than for every glyph.
    class Parameters final {
      public:
        Parameters() = default;
        Parameters(std::string effectName, std::map<std::string, std::string, std::less<>> attributes);

        [[nodiscard]] const std::string& getEffect() const noexcept {
            return effect;
        }
        [[nodiscard]] const std::map<std::string, std::string, std::less<>>& getValues() const noexcept {
            return values;
        }
        [[nodiscard]] bool has(std::string_view key) const;

        // Reads an attribute, or the default when the tag leaves it out, and throws when the attribute holds something else.
        [[nodiscard]] float getNumber(std::string_view key, float byDefault) const;
        [[nodiscard]] math::Color getColor(std::string_view key, math::Color byDefault) const;

      private:
        std::string effect;
        std::map<std::string, std::string, std::less<>> values;
        std::map<std::string, float, std::less<>> numbers;
        std::map<std::string, math::Color, std::less<>> colors;
    };

    using Function = std::function<void(Glyph& glyph, const Parameters& parameters)>;

    static void wave(Glyph& glyph, const Parameters& parameters);
    static void shake(Glyph& glyph, const Parameters& parameters);
    static void tornado(Glyph& glyph, const Parameters& parameters);
    static void fade(Glyph& glyph, const Parameters& parameters);
    static void rainbow(Glyph& glyph, const Parameters& parameters);
    static void pulse(Glyph& glyph, const Parameters& parameters);

  private:
    // Positions along the line shift the phase of the moving effects, one full turn every this many pixels times tau.
    static constexpr float kPhaseSpan = 50.0F;

    [[nodiscard]] static float ease(float value, float curve) noexcept;
    [[nodiscard]] static float pingPong(float value, float length) noexcept;
    [[nodiscard]] static float randomAngle(std::size_t character, std::int64_t step) noexcept;
};

} // namespace haylen::text
