#include "ui/components/containers/Splitter.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <ranges>
#include <utility>

#include <imgui.h>

#include "haylen/core/JsonNumber.hpp"
#include "haylen/ui/Context.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

void Splitter::readProperties(PropertyReader& reader) {
    reader.read("ratio", ratio, 0.05F, 0.95F);
    reader.read("vertical", vertical);
}

// Side by side, the children measure at the shares of the width they draw at, which the splitter takes whole, or at their natural widths in an unbounded width such as the one of a horizontal scroll. Stacked, both take the whole width and their heights add up.
math::Vec2 Splitter::measureContent(Context& context, float availableWidth) {
    auto visible = getLayoutChildren();
    auto second = std::ranges::next(visible.begin(), 1, visible.end());
    const float gutter = context.getMetric(Theme::Metric::SplitterSize);
    const bool shared = !vertical && availableWidth < CommonProperties::kUnbounded;
    const float first = shared ? std::floor(std::max(0.0F, availableWidth - gutter) * ratio) : availableWidth;
    const float rest = shared ? std::max(0.0F, availableWidth - gutter - first) : availableWidth;
    const math::Vec2 before = visible.empty() ? math::Vec2{} : visible.front()->measure(context, first);
    const math::Vec2 after = second != visible.end() ? (*second)->measure(context, rest) : math::Vec2{};

    if (vertical) {
        return {std::max(before.x, after.x), before.y + gutter + after.y};
    }
    return {shared ? availableWidth : before.x + gutter + after.x, std::max(before.y, after.y)};
}

void Splitter::render(Context& context, const math::Rect& bounds) {
    const float span = vertical ? bounds.height : bounds.width;
    const float gutter = context.getMetric(Theme::Metric::SplitterSize);
    const float first = std::floor((span - gutter) * ratio);
    const math::Rect handle = vertical ? math::Rect{bounds.x, bounds.y + first, bounds.width, gutter} : context.mirror({bounds.x + first, bounds.y, gutter, bounds.height}, bounds);

    const Widgets::Interaction state = Widgets::interact(context, handle, 0.0F, "##handle");
    if (state.hovered || state.held) {
        ImGui::SetMouseCursor(vertical ? ImGuiMouseCursor_ResizeNS : ImGuiMouseCursor_ResizeEW);
    }
    if (state.held) {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const float offset = vertical ? mouse.y - bounds.y : (context.isRightToLeft() ? bounds.getRight() - mouse.x : mouse.x - bounds.x);
        ratio = std::clamp((offset - gutter * 0.5F) / std::max(1.0F, span - gutter), 0.05F, 0.95F);
        dragging = true;
    } else if (std::exchange(dragging, false)) {
        context.emit(*this, "resize", {{"ratio", core::JsonNumber::fromFloat(ratio)}});
    }
    if (const std::optional<FocusDirection> direction = takeFocusDirection(context)) {
        const bool backward = direction == FocusDirection::Up || (direction != FocusDirection::Down && Widgets::getStep(context, *direction) < 0);
        ratio = std::clamp(ratio + (backward ? -kFocusStep : kFocusStep), 0.05F, 0.95F);
        context.emit(*this, "resize", {{"ratio", core::JsonNumber::fromFloat(ratio)}});
    }
    const math::Vec2 center = handle.getCenter();
    const math::Color color = context.getColor(state.held ? Theme::Color::Accent : Theme::Color::BorderStrong);
    ImGui::GetWindowDrawList()->AddLine(vertical ? ImVec2{handle.x, center.y} : ImVec2{center.x, handle.y}, vertical ? ImVec2{handle.getRight(), center.y} : ImVec2{center.x, handle.getBottom()}, ImGuiConverter::toImU32(color), context.getMetric(Theme::Metric::BorderWidth));

    auto visible = getLayoutChildren();
    const auto second = std::ranges::next(visible.begin(), 1, visible.end());
    const math::Rect before = vertical ? math::Rect{bounds.x, bounds.y, bounds.width, first} : context.mirror({bounds.x, bounds.y, first, bounds.height}, bounds);
    const math::Rect after = vertical ? math::Rect::fromMinMax({bounds.x, handle.getBottom()}, bounds.getMax()) : context.mirror(math::Rect::fromMinMax({bounds.x + first + gutter, bounds.y}, bounds.getMax()), bounds);
    if (!visible.empty()) {
        visible.front()->draw(context, before);
    }
    if (second != visible.end()) {
        (*second)->draw(context, after);
    }
}

} // namespace haylen::ui
