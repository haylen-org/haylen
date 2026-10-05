#pragma once

#include <optional>

#include <imgui.h>

#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Insets.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Theme.hpp"

namespace haylen::ui {

class Context;

// Paints theme surfaces and pictures. A surface uses the theme image when the theme has one, and otherwise a rounded rectangle in the flat colors.
class Surfaces final {
  public:
    // Paints a surface, whose flat colors round the corners the flags name, all of them by default.
    static void draw(Context& context, Theme::Surface role, const math::Rect& bounds, math::Color fill, std::optional<math::Color> border = std::nullopt, float radius = -1.0F, ImDrawFlags corners = ImDrawFlags_RoundCornersAll);

    // Paints the soft shadow of the theme under a floating surface, such as a dialog or a menu, reaching past the clip of its window.
    static void drawShadow(Context& context, const math::Rect& bounds, float radius);

    // Returns the radius of a rounded rectangle inside a rounded container, an inset away from its edges, so the two curves stay parallel.
    [[nodiscard]] static float getInnerRadius(float radius, float inset) noexcept;
    [[nodiscard]] static math::Insets getPadding(Context& context, Theme::Surface role);

    // Returns how far the frame of a surface reaches into its bounds: the padding of its image, or the border of its flat colors.
    [[nodiscard]] static math::Insets getFrame(Context& context, Theme::Surface role);
    static void drawNineSlice(Context& context, const Theme::Image& image, const math::Rect& bounds, math::Color fill);
    static void drawImage(Context& context, const graphics::Texture& texture, const math::Rect& bounds, math::Color tint = math::Color::white(), math::Rect source = {});

  private:
    static void drawPiece(ImDrawList& list, ImTextureRef texture, const math::Rect& destination, const math::Rect& source, math::Vec2 textureSize, ImU32 color);
};

} // namespace haylen::ui
