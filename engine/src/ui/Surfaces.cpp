#include "ui/Surfaces.hpp"

#include <algorithm>
#include <array>

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

void Surfaces::drawTiledPiece(ImDrawList& list, ImTextureRef texture, const math::Rect& destination, const math::Rect& source, math::Vec2 textureSize, float scale, ImU32 color) {
    const math::Vec2 tile{source.width * scale, source.height * scale};
    if (tile.x <= 0.0F || tile.y <= 0.0F) {
        return;
    }
    for (float y = destination.y; y < destination.getBottom(); y += tile.y) {
        const float height = std::min(tile.y, destination.getBottom() - y);
        for (float x = destination.x; x < destination.getRight(); x += tile.x) {
            const float width = std::min(tile.x, destination.getRight() - x);
            const math::Rect cut{source.x, source.y, source.width * width / tile.x, source.height * height / tile.y};
            drawPiece(list, texture, {x, y, width, height}, cut, textureSize, color);
        }
    }
}

void Surfaces::drawNineSlice(Context& context, const Theme::Image& image, const math::Rect& bounds, math::Color fill) {
    const graphics::Texture& texture = image.slice.texture;
    if (!texture.isValid() || bounds.width <= 0.0F || bounds.height <= 0.0F) {
        return;
    }

    // Borders keep their scaled size unless the bounds are too small for both sides, and then they shrink together.
    const std::array<math::Rect, 9>& pieces = image.slice.pieces;
    math::Insets borders{pieces[3].width * image.scale, pieces[1].height * image.scale, pieces[5].width * image.scale, pieces[7].height * image.scale};
    const float shrinkX = borders.getHorizontal() > bounds.width ? bounds.width / borders.getHorizontal() : 1.0F;
    const float shrinkY = borders.getVertical() > bounds.height ? bounds.height / borders.getVertical() : 1.0F;
    borders = {borders.left * shrinkX, borders.top * shrinkY, borders.right * shrinkX, borders.bottom * shrinkY};

    const std::array<float, 4> xs{bounds.x, bounds.x + borders.left, bounds.getRight() - borders.right, bounds.getRight()};
    const std::array<float, 4> ys{bounds.y, bounds.y + borders.top, bounds.getBottom() - borders.bottom, bounds.getBottom()};
    const ImTextureRef reference = context.getTextureReference(texture);
    const ImU32 color = ImGuiConverter::toImU32(image.colorize ? image.tint * fill : image.tint);
    ImDrawList& list = *ImGui::GetWindowDrawList();
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            const math::Rect destination = math::Rect::fromMinMax({xs[column], ys[row]}, {xs[column + 1], ys[row + 1]});
            const math::Rect& source = pieces[row * 3 + column];
            const bool stretchedEdge = row == 1 || column == 1;
            if (image.slice.fill == graphics2d::NineSlice::Fill::Tile && stretchedEdge) {
                drawTiledPiece(list, reference, destination, source, texture.getSize(), image.scale, color);
            } else {
                drawPiece(list, reference, destination, source, texture.getSize(), color);
            }
        }
    }
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
