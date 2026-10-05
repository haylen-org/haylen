#pragma once

#include <array>
#include <optional>

#include <imgui.h>

#include "haylen/2d/graphics/Shape.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Insets.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Theme.hpp"

namespace haylen::ui {

class Context;

// Paints theme surfaces, shapes and pictures. A surface uses the theme image when the theme has one, and otherwise a rounded rectangle in the flat colors, drawn like every curve of the UI as a shape whose edges fade over one pixel of the screen.
class Surfaces final {
  public:
    // Paints a surface, whose flat colors round the corners the flags name, all of them by default, with the border inside the same edge.
    static void draw(Context& context, Theme::Surface role, const math::Rect& bounds, math::Color fill, std::optional<math::Color> border = std::nullopt, float radius = -1.0F, ImDrawFlags corners = ImDrawFlags_RoundCornersAll);

    // Fills a rectangle whose corners the flags name round by the radius, up to half its shorter side, such as the highlight of a row.
    static void fill(Context& context, const math::Rect& bounds, math::Color color, float radius, ImDrawFlags corners = ImDrawFlags_RoundCornersAll);

    // Draws a border of the width inside a rectangle whose corners round by the radius, up to half its shorter side, such as the outline of the target of a drag.
    static void outline(Context& context, const math::Rect& bounds, math::Color color, float radius, float width);

    // Draws a shape in UI coordinates, moved, scaled and faded by the transforms of the nodes around it and by the alpha of ImGui, such as the alpha of a disabled node.
    static void drawShape(Context& context, graphics2d::Shape shape);

    // Paints the soft shadow of the theme under a floating surface, such as a dialog or a menu, reaching past the clip of its window.
    static void drawShadow(Context& context, const math::Rect& bounds, float radius);

    // Returns the radius of each corner, from the top-left one clockwise, for the corners the flags round.
    [[nodiscard]] static std::array<float, 4> getRadii(float radius, ImDrawFlags corners) noexcept;

    // Returns the radius of a rounded rectangle inside a rounded container, an inset away from its edges, so the two curves stay parallel.
    [[nodiscard]] static float getInnerRadius(float radius, float inset) noexcept;
    [[nodiscard]] static math::Insets getPadding(Context& context, Theme::Surface role);

    // Returns how far the frame of a surface reaches into its bounds: the padding of its image, or the border of its flat colors.
    [[nodiscard]] static math::Insets getFrame(Context& context, Theme::Surface role);
    static void drawNineSlice(Context& context, const Theme::Image& image, const math::Rect& bounds, math::Color fill);
    static void drawImage(Context& context, const graphics::Texture& texture, const math::Rect& bounds, math::Color tint = math::Color::white(), math::Rect source = {}, float radius = 0.0F);

  private:
    static void drawPiece(ImDrawList& list, ImTextureRef texture, const math::Rect& destination, const math::Rect& source, math::Vec2 textureSize, ImU32 color, float radius);
};

} // namespace haylen::ui
