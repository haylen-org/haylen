#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "haylen/core/Json.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/ui/Theme.hpp"

namespace haylen::ui {

// Values that replace those of the theme for one node and every node inside it: colors, metrics, fonts and surfaces, in the format of the sections of a theme file. A value the style does not set comes from the theme around it.
class Style final {
  public:
    // The parts of a font role a style sets, where the others come from the font role around it.
    struct Font {
        std::optional<std::string> font;
        std::optional<float> size;
        std::optional<bool> bold;
        std::optional<bool> italic;
    };

    // Reads a style from an object with `colors`, `metrics`, `fonts` and `surfaces` at the path of the property that holds it, such as `button.style`, which starts its errors.
    [[nodiscard]] static Style fromJson(const core::Json& value, std::string_view path);

    [[nodiscard]] const std::optional<math::Color>& findColor(Theme::Color role) const noexcept {
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

  private:
    // A surface of the style: its definition, or null for flat colors, and its image once loaded.
    struct Surface {
        core::Json definition;
        std::string context;
        mutable std::optional<Theme::Image> image;
    };

    static void readFonts(Style& style, const core::Json& section, const std::string& subject, const std::string& inside);
    [[nodiscard]] static const core::Json& readSection(const core::Json& value, const char* key, const std::string& subject);

    std::array<std::optional<math::Color>, Theme::kColorCount> colors{};
    std::array<std::optional<float>, Theme::kMetricCount> metrics{};
    std::array<Font, Theme::kFontCount> fonts{};
    std::array<std::optional<Surface>, Theme::kSurfaceCount> surfaces{};
};

} // namespace haylen::ui
