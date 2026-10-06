#include "ui/Surfaces.hpp"

#include <algorithm>

#include "haylen/ui/Backend.hpp"
#include "haylen/ui/Context.hpp"
#include "ui/ImGuiConverter.hpp"

namespace haylen::ui {

void Surfaces::drawPiece(ImDrawList& list, ImTextureRef texture, const math::Rect& destination, const math::Rect& source, math::Vec2 textureSize, ImU32 color, float radius) {
    if (destination.width <= 0.0F || destination.height <= 0.0F || source.width <= 0.0F || source.height <= 0.0F) {
        return;
    }
    const ImVec2 uvMin{source.x / textureSize.x, source.y / textureSize.y};
    const ImVec2 uvMax{source.getRight() / textureSize.x, source.getBottom() / textureSize.y};
    if (radius > 0.0F) {
        list.AddImageRounded(texture, ImGuiConverter::toImVec2(destination.getMin()), ImGuiConverter::toImVec2(destination.getMax()), uvMin, uvMax, color, std::min(radius, std::min(destination.width, destination.height) * 0.5F));
        return;
    }
    list.AddImage(texture, ImGuiConverter::toImVec2(destination.getMin()), ImGuiConverter::toImVec2(destination.getMax()), uvMin, uvMax, color);
}

// The frame draws through the 2D renderer at its place among the ImGui draws of the window, so its pieces lay out and sample like every other nine-slice, and the transform of the node moves them like the vertices around them.
void Surfaces::drawNineSlice(Context& context, const Theme::Image& image, const math::Rect& bounds, math::Color fill) {
    if (!image.slice.isValid() || bounds.isEmpty()) {
        return;
    }

    const Context::Reshape shape = context.getReshape();
    const math::Color tinted = (image.colorize ? image.tint * fill : image.tint) * shape.color;
    const math::Color color = tinted.withAlpha(tinted.a * ImGui::GetStyle().Alpha);

    // The command of the draw carries the texture of the frame, so the draw list of the window tells which image it shows where, like its other commands.
    ImDrawList& list = *ImGui::GetWindowDrawList();
    list.PushTexture(context.getTextureReference(image.slice.texture));
    context.getBackend().addNineSlice(image.slice, bounds, image.scale, shape.scale, shape.offset, color);
    list.PopTexture();
}

// The border runs inside the edge of the surface, so it never reaches past its bounds and follows its corners exactly.
void Surfaces::draw(Context& context, Theme::Surface role, const math::Rect& bounds, math::Color fill, std::optional<math::Color> border, float radius, ImDrawFlags corners) {
    if (const Theme::Image* image = context.getSurface(role)) {
        drawNineSlice(context, *image, bounds, fill);
        return;
    }
    const float rounding = std::min(radius < 0.0F ? context.getMetric(Theme::Metric::ControlRadius) : radius, std::min(bounds.width, bounds.height) * 0.5F);
    const float width = context.getMetric(Theme::Metric::BorderWidth);
    const bool bordered = border && border->a > 0.0F && width > 0.0F;
    if (fill.a <= 0.0F && !bordered) {
        return;
    }
    drawShape(context, {.bounds = bounds, .radii = getRadii(rounding, corners), .color = fill, .borderWidth = bordered ? width : 0.0F, .borderColor = bordered ? *border : math::Color::transparent()});
}

// A shape rounds each corner by at most half its shorter side, so the radius needs no limit here.
void Surfaces::fill(Context& context, const math::Rect& bounds, math::Color color, float radius, ImDrawFlags corners) {
    if (color.a > 0.0F) {
        drawShape(context, {.bounds = bounds, .radii = getRadii(radius, corners), .color = color});
    }
}

void Surfaces::outline(Context& context, const math::Rect& bounds, math::Color color, float radius, float width) {
    if (color.a > 0.0F && width > 0.0F) {
        drawShape(context, {.bounds = bounds, .radii = getRadii(radius, ImDrawFlags_RoundCornersAll), .color = math::Color::transparent(), .borderWidth = width, .borderColor = color});
    }
}

void Surfaces::drawShape(Context& context, graphics2d::Shape shape) {
    const Context::Reshape reshape = context.getReshape();
    const float scale = std::min(reshape.scale.x, reshape.scale.y);
    const math::Color tint = reshape.color.withAlpha(reshape.color.a * ImGui::GetStyle().Alpha);
    shape.bounds = reshape.apply(shape.bounds);
    for (float& radius : shape.radii) {
        radius *= scale;
    }
    shape.borderWidth *= scale;
    shape.softness *= scale;
    shape.color = shape.color * tint;
    shape.borderColor = shape.borderColor * tint;
    context.getBackend().addShape(shape);
}

std::array<float, 4> Surfaces::getRadii(float radius, ImDrawFlags corners) noexcept {
    const auto rounded = [radius, corners](ImDrawFlags corner) { return (corners & corner) != 0 ? radius : 0.0F; };
    return {rounded(ImDrawFlags_RoundCornersTopLeft), rounded(ImDrawFlags_RoundCornersTopRight), rounded(ImDrawFlags_RoundCornersBottomRight), rounded(ImDrawFlags_RoundCornersBottomLeft)};
}

float Surfaces::getInnerRadius(float radius, float inset) noexcept {
    return std::max(0.0F, radius - inset);
}

// The shadow is the surface moved down by the offset and grown by half the size of the shadow, with an edge that fades over the whole size, so it is as dark as its color along the edge of the surface and clears the size away from it.
void Surfaces::drawShadow(Context& context, const math::Rect& bounds, float radius) {
    const float size = context.getMetric(Theme::Metric::ShadowSize);
    const math::Color color = context.getColor(Theme::Color::Shadow);
    if (size <= 0.0F || color.a <= 0.0F) {
        return;
    }
    const math::Rect cast = bounds.translated({0.0F, context.getMetric(Theme::Metric::ShadowOffset)}).expanded(size * 0.5F);
    const float rounding = radius + size * 0.5F;
    ImDrawList& list = *ImGui::GetWindowDrawList();
    const math::Rect reach = context.getReshape().apply(cast.expanded(size * 0.5F));
    list.PushClipRect(ImGuiConverter::toImVec2(reach.getMin()), ImGuiConverter::toImVec2(reach.getMax()), false);
    drawShape(context, {.bounds = cast, .radii = {rounding, rounding, rounding, rounding}, .color = color, .softness = size});
    list.PopClipRect();
}

math::Insets Surfaces::getPadding(Context& context, Theme::Surface role) {
    const Theme::Image* image = context.getSurface(role);
    return image != nullptr ? image->padding : math::Insets{};
}

math::Insets Surfaces::getFrame(Context& context, Theme::Surface role) {
    const Theme::Image* image = context.getSurface(role);
    return image != nullptr ? image->padding : math::Insets::uniform(context.getMetric(Theme::Metric::BorderWidth));
}

void Surfaces::drawImage(Context& context, const graphics::Texture& texture, const math::Rect& bounds, math::Color tint, math::Rect source, float radius) {
    if (!texture.isValid()) {
        return;
    }
    const math::Rect area = source.isEmpty() ? math::Rect{0.0F, 0.0F, texture.getSize().x, texture.getSize().y} : source;
    drawPiece(*ImGui::GetWindowDrawList(), context.getTextureReference(texture), bounds, area, texture.getSize(), ImGuiConverter::toImU32(tint), radius);
}

} // namespace haylen::ui
