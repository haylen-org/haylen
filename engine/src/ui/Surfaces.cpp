#include "ui/Surfaces.hpp"

#include <utility>
#include <vector>

#include "haylen/2d/graphics/NineSlice.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/graphics/SpriteInstance.hpp"
#include "haylen/ui/Backend.hpp"
#include "haylen/ui/Context.hpp"
#include "ui/ImGuiConverter.hpp"

namespace haylen::ui {

void Surfaces::drawPiece(ImDrawList& list, ImTextureRef texture, const math::Rect& destination, const math::Rect& source, math::Vec2 textureSize, ImU32 color) {
    if (destination.width <= 0.0F || destination.height <= 0.0F || source.width <= 0.0F || source.height <= 0.0F) {
        return;
    }
    const ImVec2 uvMin{source.x / textureSize.x, source.y / textureSize.y};
    const ImVec2 uvMax{source.getRight() / textureSize.x, source.getBottom() / textureSize.y};
    list.AddImage(texture, ImGuiConverter::toImVec2(destination.getMin()), ImGuiConverter::toImVec2(destination.getMax()), uvMin, uvMax, color);
}

// The frame draws through the 2D renderer at its place among the ImGui draws of the window, so its pieces lay out and sample like every other nine-slice, and the transform of the node moves them like the vertices around them.
void Surfaces::drawNineSlice(Context& context, const Theme::Image& image, const math::Rect& bounds, math::Color fill) {
    if (!image.slice.isValid() || bounds.isEmpty()) {
        return;
    }

    std::vector<graphics2d::NineSlice::Patch> patches;
    image.slice.layout(bounds, image.scale, patches);
    const Context::Reshape shape = context.getReshape();
    const math::Color tinted = (image.colorize ? image.tint * fill : image.tint) * shape.color;
    const math::Color color = tinted.withAlpha(tinted.a * ImGui::GetStyle().Alpha);
    std::vector<graphics2d::SpriteInstance> sprites;
    sprites.reserve(patches.size());
    for (const graphics2d::NineSlice::Patch& patch : patches) {
        sprites.push_back({.position = patch.area.getMin() * shape.scale + shape.offset, .size = patch.area.getSize() * shape.scale, .source = patch.source, .pivot = {}, .color = color});
    }

    // The command of the callback carries the texture of the frame, so the draw list of the window tells which image it shows where, like its other commands.
    ImDrawList& list = *ImGui::GetWindowDrawList();
    list.PushTexture(context.getTextureReference(image.slice.texture));
    // clang-format off
    context.getBackend().addRenderCallback([texture = image.slice.texture, sprites = std::move(sprites)](graphics2d::Renderer& renderer, math::Vec2 offset) mutable {
        for (graphics2d::SpriteInstance& sprite : sprites) {
            sprite.position += offset;
        }
        renderer.drawBatch(texture, sprites);
    });
    // clang-format on
    list.PopTexture();
}

void Surfaces::draw(Context& context, Theme::Surface role, const math::Rect& bounds, math::Color fill, std::optional<math::Color> border, float radius) {
    if (const Theme::Image* image = context.getTheme().getSurface(role)) {
        drawNineSlice(context, *image, bounds, fill);
        return;
    }
    const float rounding = radius < 0.0F ? context.getMetric(Theme::Metric::ControlRadius) : radius;
    ImDrawList& list = *ImGui::GetWindowDrawList();
    if (fill.a > 0.0F) {
        list.AddRectFilled(ImGuiConverter::toImVec2(bounds.getMin()), ImGuiConverter::toImVec2(bounds.getMax()), ImGuiConverter::toImU32(fill), rounding);
    }
    if (border && border->a > 0.0F) {
        list.AddRect(ImGuiConverter::toImVec2(bounds.getMin()), ImGuiConverter::toImVec2(bounds.getMax()), ImGuiConverter::toImU32(*border), rounding, context.getMetric(Theme::Metric::BorderWidth));
    }
}

math::Insets Surfaces::getPadding(Context& context, Theme::Surface role) {
    const Theme::Image* image = context.getTheme().getSurface(role);
    return image != nullptr ? image->padding : math::Insets{};
}

void Surfaces::drawImage(Context& context, const graphics::Texture& texture, const math::Rect& bounds, math::Color tint, math::Rect source) {
    if (!texture.isValid()) {
        return;
    }
    const math::Rect area = source.isEmpty() ? math::Rect{0.0F, 0.0F, texture.getSize().x, texture.getSize().y} : source;
    drawPiece(*ImGui::GetWindowDrawList(), context.getTextureReference(texture), bounds, area, texture.getSize(), ImGuiConverter::toImU32(tint));
}

} // namespace haylen::ui
