#include "ui/components/containers/Splitter.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>
#include <vector>

#include <imgui.h>

#include "haylen/ui/Context.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

void Splitter::readProperties(PropertyReader& reader) {
    reader.read("ratio", ratio, 0.05F, 0.95F);
    reader.read("vertical", vertical);
}

math::Vec2 Splitter::measureContent(Context& context, float availableWidth) {
    math::Vec2 size;
    for (Component* child : getLayoutChildren()) {
        size = math::Vec2::max(size, child->measure(context, availableWidth));
    }
    return size;
}

void Splitter::render(Context& context, const math::Rect& bounds) {
    constexpr float kHandle = 10.0F;
    const float span = vertical ? bounds.height : bounds.width;
    const float first = std::floor((span - kHandle) * ratio);
    const math::Rect handle = vertical ? math::Rect{bounds.x, bounds.y + first, bounds.width, kHandle} : math::Rect{bounds.x + first, bounds.y, kHandle, bounds.height};

    const Widgets::Interaction state = Widgets::interact(context, handle, 0.0F, "##handle");
    if (state.hovered || state.held) {
        ImGui::SetMouseCursor(vertical ? ImGuiMouseCursor_ResizeNS : ImGuiMouseCursor_ResizeEW);
    }
    if (state.held) {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const float offset = vertical ? mouse.y - bounds.y : mouse.x - bounds.x;
        ratio = std::clamp((offset - kHandle * 0.5F) / std::max(1.0F, span - kHandle), 0.05F, 0.95F);
        dragging = true;
    } else if (std::exchange(dragging, false)) {
        context.emit(*this, "resize", {{"ratio", ratio}});
    }
    if (const std::optional<FocusDirection> direction = takeFocusDirection(context)) {
        const bool backward = direction == FocusDirection::Left || direction == FocusDirection::Up;
        ratio = std::clamp(ratio + (backward ? -kFocusStep : kFocusStep), 0.05F, 0.95F);
        context.emit(*this, "resize", {{"ratio", ratio}});
    }
    const math::Vec2 center = handle.getCenter();
    const math::Color color = context.getColor(state.held ? Theme::Color::Accent : Theme::Color::BorderStrong);
    ImGui::GetWindowDrawList()->AddLine(vertical ? ImVec2{handle.x, center.y} : ImVec2{center.x, handle.y}, vertical ? ImVec2{handle.getRight(), center.y} : ImVec2{center.x, handle.getBottom()}, ImGuiConverter::toImU32(color), context.getMetric(Theme::Metric::BorderWidth));

    const std::vector<Component*> visible = getLayoutChildren();
    const math::Rect before = vertical ? math::Rect{bounds.x, bounds.y, bounds.width, first} : math::Rect{bounds.x, bounds.y, first, bounds.height};
    const math::Rect after = vertical ? math::Rect::fromMinMax({bounds.x, handle.getBottom()}, bounds.getMax()) : math::Rect::fromMinMax({handle.getRight(), bounds.y}, bounds.getMax());
    if (!visible.empty()) {
        visible[0]->draw(context, before);
    }
    if (visible.size() > 1) {
        visible[1]->draw(context, after);
    }
}

} // namespace haylen::ui
