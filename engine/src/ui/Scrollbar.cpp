#include "ui/Scrollbar.hpp"

#include <algorithm>

#include <imgui.h>
#include <imgui_internal.h>

#include "haylen/ui/Backend.hpp"
#include "haylen/ui/Context.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/Surfaces.hpp"

namespace haylen::ui {

float Scrollbar::getGap(float metric, float pointsPerUnit) noexcept {
    return std::max(metric, kMinimumGap / pointsPerUnit);
}

float Scrollbar::getGap(const Context& context) {
    return getGap(context.getMetric(Theme::Metric::ScrollbarGap), context.getBackend().getPointsPerUnit());
}

float Scrollbar::getLane(const Context& context) {
    return getGap(context) + context.getMetric(Theme::Metric::ScrollbarSize) + context.getMetric(Theme::Metric::ScrollbarInset);
}

float Scrollbar::getReserve(const Context& context, float padding) {
    return std::max(0.0F, getLane(context) - padding);
}

math::Rect Scrollbar::getContentBox(const Context& context, const math::Rect& area, const math::Rect& box, bool horizontal) {
    const float lane = getLane(context);
    if (horizontal) {
        return math::Rect::fromMinMax(box.getMin(), {box.getRight(), std::max(box.y, std::min(box.getBottom(), area.getBottom() - lane))});
    }
    if (context.isRightToLeft()) {
        return math::Rect::fromMinMax({std::min(box.getRight(), std::max(box.x, area.x + lane)), box.y}, box.getMax());
    }
    return math::Rect::fromMinMax(box.getMin(), {std::max(box.x, std::min(box.getRight(), area.getRight() - lane)), box.getBottom()});
}

std::optional<double> Scrollbar::interact(Context& context, const math::Rect& area, bool horizontal, bool reversed, double offset, double maximum, float viewLength, float radius) {
    // The bar runs along the end edge an inset away from it, and its ends keep the inset from the other edges and, where the area rounds its corners, from the curve of each corner.
    const float size = context.getMetric(Theme::Metric::ScrollbarSize);
    const float inset = context.getMetric(Theme::Metric::ScrollbarInset);
    const float lane = getLane(context);
    const float end = std::max(inset, radius - size * 0.5F);
    const bool atLeft = !horizontal && context.isRightToLeft();
    math::Rect hit;
    if (horizontal) {
        track = {area.x + end, area.getBottom() - inset - size, std::max(0.0F, area.width - end * 2.0F), size};
        hit = {area.x, area.getBottom() - lane, area.width, lane};
    } else {
        track = {atLeft ? area.x + inset : area.getRight() - inset - size, area.y + end, size, std::max(0.0F, area.height - end * 2.0F)};
        hit = {atLeft ? area.x : area.getRight() - lane, area.y, lane, area.height};
    }

    const float trackLength = horizontal ? track.width : track.height;
    const auto length = static_cast<float>(std::clamp(static_cast<double>(trackLength) * viewLength / (maximum + viewLength), static_cast<double>(std::min(kMinThumb, trackLength)), static_cast<double>(trackLength)));
    const float travel = trackLength - length;
    const float progress = maximum > 0.0 ? static_cast<float>(std::clamp(offset / maximum, 0.0, 1.0)) : 0.0F;
    const float start = (reversed ? 1.0F - progress : progress) * travel;
    thumb = horizontal ? math::Rect{track.x + start, track.y, length, size} : math::Rect{track.x, track.y + start, size, length};

    const ImGuiID id = ImGui::GetID("##scrollbar");
    const ImRect box = ImGuiConverter::toImRect(hit);
    if (!ImGui::ItemAdd(box, id)) {
        hovered = held = grabbing = false;
        return std::nullopt;
    }
    const bool pressed = ImGui::ButtonBehavior(box, id, &hovered, &held, ImGuiButtonFlags_NoNavFocus | ImGuiButtonFlags_PressedOnClick);
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const float along = horizontal ? mouse.x - track.x : mouse.y - track.y;

    // A press on the thumb grabs it where it was pressed, and a press elsewhere in the lane moves by one view toward the press.
    if (pressed) {
        grabbing = along >= start && along <= start + length;
        grab = along - start;
        if (!grabbing) {
            const bool backward = (along < start) != reversed;
            return std::clamp(offset + (backward ? -viewLength : viewLength), 0.0, maximum);
        }
    }
    if (!held) {
        grabbing = false;
    }
    if (!grabbing || travel <= 0.0F) {
        return std::nullopt;
    }
    const double ratio = std::clamp(static_cast<double>((along - grab) / travel), 0.0, 1.0);
    return (reversed ? 1.0 - ratio : ratio) * maximum;
}

// A hovered or held thumb takes the hover surface, or the normal one when the theme has no hover image.
void Scrollbar::draw(Context& context, float alpha) const {
    if (alpha <= 0.0F || thumb.isEmpty()) {
        return;
    }
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * alpha);
    if (const Theme::Image* image = context.getSurface(Theme::Surface::ScrollbarTrack)) {
        Surfaces::drawNineSlice(context, *image, track, context.getColor(Theme::Color::Track));
    }
    const bool active = hovered || held;
    const Theme::Surface surface = active && context.getSurface(Theme::Surface::ScrollbarHover) != nullptr ? Theme::Surface::ScrollbarHover : Theme::Surface::Scrollbar;
    const float radius = std::min(thumb.width, thumb.height) * 0.5F;
    Surfaces::draw(context, surface, thumb, context.getColor(active ? Theme::Color::ScrollbarHover : Theme::Color::Scrollbar), std::nullopt, radius);
    ImGui::PopStyleVar();
}

} // namespace haylen::ui
