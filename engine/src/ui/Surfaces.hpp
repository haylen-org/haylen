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
    static void draw(Context& context, Theme::Surface role, const math::Rect& bounds, math::Color fill, std::optional<math::Color> border = std::nullopt, float radius = -1.0F);
    [[nodiscard]] static math::Insets getPadding(Context& context, Theme::Surface role);
    static void drawNineSlice(Context& context, const Theme::Image& image, const math::Rect& bounds, math::Color fill);
    static void drawImage(Context& context, const graphics::Texture& texture, const math::Rect& bounds, math::Color tint = math::Color::white(), math::Rect source = {});

  private:
    static void drawPiece(ImDrawList& list, ImTextureRef texture, const math::Rect& destination, const math::Rect& source, math::Vec2 textureSize, ImU32 color);
};

} // namespace haylen::ui
