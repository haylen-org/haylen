#pragma once

#include <array>
#include <bitset>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "haylen/core/Json.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/ui/Theme.hpp"

namespace haylen::ui {

// Values that replace those of the theme for one node and every node inside it: colors, metrics, fonts, surfaces and sounds, in the format of the sections of a theme file. A value the style does not set comes from the theme around it.
class Style final {
  public:
    // The parts of a font role a style sets, where the others come from the font role around it.
    struct Font {
        std::optional<std::string> font;
        std::optional<float> size;
        std::optional<bool> bold;
        std::optional<bool> italic;
        std::optional<bool> ligatures;
    };

    // A color the style sets: a color of its own, or a theme color role, which takes the color of that role around the style each time it draws.
    using Color = std::variant<math::Color, Theme::Color>;

    // Reads a style from an object with `colors`, `metrics`, `fonts`, `surfaces` and `sounds` at the path of the property that holds it, such as `button.style`, which starts its errors.
    [[nodiscard]] static Style fromJson(const core::Json& value, std::string_view path);

    [[nodiscard]] const std::optional<Color>& findColor(Theme::Color role) const noexcept {
        return colors[static_cast<std::size_t>(role)];
    }
    [[nodiscard]] const std::optional<float>& findMetric(Theme::Metric role) const noexcept {
        return metrics[static_cast<std::size_t>(role)];
    }
    [[nodiscard]] const Font& getFont(Theme::Font role) const noexcept {
        return fonts[static_cast<std::size_t>(role)];
    }

    // Whether the style sets a surface, which it may also set to flat colors.
    [[nodiscard]] bool hasSurface(Theme::Surface role) const noexcept {
        return surfaces[static_cast<std::size_t>(role)].has_value();
    }

    // Returns the image of a surface the style sets, loading its texture with the loader, or null while the texture loads or when the style sets flat colors. The image is kept once its texture arrived.
    [[nodiscard]] const Theme::Image* findSurface(Theme::Surface role, const Theme::TextureLoader& loadTexture) const;

    // Whether the style sets the sound of an interaction, which it may also set to play nothing.
    [[nodiscard]] bool hasSound(Theme::Sound role) const noexcept {
        return soundsSet.test(static_cast<std::size_t>(role));
    }

    // Returns the sound the style gives an interaction, or null when the style does not set it or sets it to play nothing.
    [[nodiscard]] const Theme::Cue* findSound(Theme::Sound role) const noexcept;

    // Hands every sound file of the style to the loader the first time it is called, so a node starts loading its sounds when it first draws.
    void loadSounds(const std::function<void(std::string_view path)>& load) const;

  private:
    // A surface of the style: its definition, or null for flat colors, and its image once loaded.
    struct Surface {
        core::Json definition;
        std::string context;
        mutable std::optional<Theme::Image> image;
    };

    static void readFonts(Style& style, const core::Json& section, const std::string& subject, const std::string& inside);
    static void readSounds(Style& style, const core::Json& section, const std::string& subject, const std::string& inside);
    [[nodiscard]] static const core::Json& readSection(const core::Json& value, const char* key, const std::string& subject);

    [[nodiscard]] static Color readColor(const core::Json& value, const std::string& context);

    std::array<std::optional<Color>, Theme::kColorCount> colors{};
    std::array<std::optional<float>, Theme::kMetricCount> metrics{};
    std::array<Font, Theme::kFontCount> fonts{};
    std::array<std::optional<Surface>, Theme::kSurfaceCount> surfaces{};
    std::array<std::optional<Theme::Cue>, Theme::kSoundCount> sounds{};
    std::bitset<Theme::kSoundCount> soundsSet;
    mutable bool soundsLoaded = false;
};

} // namespace haylen::ui
