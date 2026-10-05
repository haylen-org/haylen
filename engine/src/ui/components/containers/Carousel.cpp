#include "ui/components/containers/Carousel.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <ranges>
#include <utility>

#include <imgui.h>

#include "haylen/ui/Context.hpp"
#include "haylen/ui/FocusNavigator.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

void Carousel::readProperties(PropertyReader& reader) {
    reader.read("page", page, 1, 1 << 20);
    reader.read("loop", loop);
    reader.read("indicators", indicators);
    reader.read("arrows", arrows);
    reader.read("interval", interval, 0.0F, 3600.0F);
}

float Carousel::getIndicatorHeight(Context& context) const {
    return indicators ? context.getMetric(Theme::Metric::PageIndicatorSize) * 3.0F : 0.0F;
}

// A carousel fills the width it gets, and an unbounded width, such as the one of a horizontal scroll, gets the width of its widest page.
math::Vec2 Carousel::measureContent(Context& context, float availableWidth) {
    math::Vec2 size;
    for (Component* child : getLayoutChildren()) {
        size = math::Vec2::max(size, child->measure(context, availableWidth));
    }
    return {availableWidth < CommonProperties::kUnbounded ? availableWidth : size.x, size.y + getIndicatorHeight(context)};
}

void Carousel::render(Context& context, const math::Rect& bounds) {
    const auto count = static_cast<std::size_t>(std::ranges::distance(getLayoutChildren()));
    if (count == 0) {
        return;
    }
    page = std::clamp(page, 1, static_cast<int>(count));
    const math::Rect area{bounds.x, bounds.y, bounds.width, std::max(0.0F, bounds.height - getIndicatorHeight(context))};

    // The arrows come before the pages, so they win the pointer over the content under them, and they paint on a layer above the pages.
    ImDrawList& list = *ImGui::GetWindowDrawList();
    ImDrawListSplitter layers;
    layers.Split(&list, 2);
    layers.SetCurrentChannel(&list, 1);
    int target = page;
    if (arrows) {
        target += drawArrows(context, area, count);
    }
    layers.SetCurrentChannel(&list, 0);

    const float goal = static_cast<float>(page - 1);
    shown += (goal - shown) * std::min(1.0F, context.getDeltaSeconds() * kSlideSpeed);
    if (std::fabs(goal - shown) < 0.001F) {
        shown = goal;
    }
    drawPages(context, area);
    layers.Merge(&list);

    // A drag over the pages follows the finger, and a release past a fifth of the width turns the page.
    ImGuiButtonFlags swiping = ImGuiButtonFlags_None;
    if (indicators) {
        swiping |= ImGuiButtonFlags_NoNavFocus;
    }
    const Widgets::Interaction swipe = Widgets::interact(context, area, 0.0F, "##swipe", swiping);
    if (!indicators && takeFocusRequest()) {
        Widgets::focusItem(context);
    }
    if (swipe.held) {
        dragged += ImGui::GetIO().MouseDelta.x;
        waited = 0.0F;
    } else if (dragged != 0.0F) {
        if (std::fabs(dragged) > area.width * kSwipeShare) {
            target = page + ((dragged < 0.0F) != context.isRightToLeft() ? 1 : -1);
        }
        shown -= (context.isRightToLeft() ? -dragged : dragged) / std::max(1.0F, area.width);
        dragged = 0.0F;
    }

    if (indicators) {
        if (const int picked = drawIndicators(context, {bounds.x, area.getBottom(), bounds.width, getIndicatorHeight(context)}, count); picked > 0) {
            target = picked;
        }
    }
    if (const std::optional<FocusDirection> direction = takeFocusDirection(context)) {
        target = page + Widgets::getStep(context, *direction);
    }
    if (interval > 0.0F && !swipe.held && target == page) {
        waited += context.getDeltaSeconds();
        if (waited >= interval) {
            const bool looped = std::exchange(loop, true);
            turn(context, page + 1, count);
            loop = looped;
            return;
        }
    }
    turn(context, target, count);
}

// Pages other than the current one draw while they slide in or out, and their controls stay out of navigation.
void Carousel::drawPages(Context& context, const math::Rect& area) {
    // The pages run from the right in a right-to-left UI, where a drag to the right moves forward.
    const float position = shown - (context.isRightToLeft() ? -dragged : dragged) / std::max(1.0F, area.width);
    ImGui::PushClipRect(ImGuiConverter::toImVec2(area.getMin()), ImGuiConverter::toImVec2(area.getMax()), true);
    std::size_t index = 0;
    for (Component* shownPage : getLayoutChildren()) {
        const float offset = static_cast<float>(index) - position;
        ++index;
        if (std::fabs(offset) >= 1.0F) {
            continue;
        }
        const bool current = static_cast<int>(index) == page;
        context.getFocus().suspendTargets(!current);
        const math::Rect placed = context.mirror({area.x + offset * area.width, area.y, area.width, area.height}, area);
        shownPage->draw(context, {std::floor(placed.x), area.y, area.width, area.height});
        context.getFocus().suspendTargets(false);
    }
    ImGui::PopClipRect();
}

int Carousel::drawArrows(Context& context, const math::Rect& area, std::size_t count) {
    const float radius = context.getMetric(Theme::Metric::ControlHeight) * 0.4F;
    const float margin = context.getMetric(Theme::Metric::ItemSpacing);
    int moved = 0;
    for (const int direction : {-1, 1}) {
        const int next = page + direction;
        if (!loop && (next < 1 || next > static_cast<int>(count))) {
            continue;
        }
        const float x = direction < 0 ? area.x + margin + radius : area.getRight() - margin - radius;
        const math::Rect button = context.mirror({x - radius, area.getCenter().y - radius, radius * 2.0F, radius * 2.0F}, area);
        ImGui::PushID(direction);
        const Widgets::Interaction state = Widgets::interact(context, button, radius, "##arrow", ImGuiButtonFlags_NoNavFocus);
        ImGui::PopID();
        const math::Color fill = context.getColor(Theme::Color::Overlay);
        ImGui::GetWindowDrawList()->AddCircleFilled(ImGuiConverter::toImVec2(button.getCenter()), radius, ImGuiConverter::toImU32(state.hovered ? fill.withAlpha(std::min(1.0F, fill.a + 0.2F)) : fill));
        Widgets::arrow(button.getCenter(), radius * 0.7F, Widgets::mirror(context, direction < 0 ? ImGuiDir_Left : ImGuiDir_Right), context.getColor(Theme::Color::OnAccent));
        if (state.clicked) {
            moved = direction;
        }
    }
    return moved;
}

int Carousel::drawIndicators(Context& context, const math::Rect& row, std::size_t count) {
    const float size = context.getMetric(Theme::Metric::PageIndicatorSize);
    const float width = size * (static_cast<float>(count) * 2.0F - 1.0F);
    const math::Rect dots{std::floor(row.getCenter().x - width * 0.5F), std::floor(row.getCenter().y - size * 0.5F), width, size};
    int picked = 0;
    for (std::size_t index = 0; index < count; ++index) {
        const math::Rect dot = context.mirror({dots.x + size * 2.0F * static_cast<float>(index), dots.y, size, size}, dots);
        ImGui::PushID(static_cast<int>(index));
        const Widgets::Interaction state = Widgets::interact(context, dot.expanded(size * 0.5F), size, "##dot", ImGuiButtonFlags_NoNavFocus);
        ImGui::PopID();
        const bool current = static_cast<int>(index) == page;
        ImGui::GetWindowDrawList()->AddCircleFilled(ImGuiConverter::toImVec2(dot.getCenter()), size * 0.5F, ImGuiConverter::toImU32(context.getColor(current ? Theme::Color::Accent : state.hovered ? Theme::Color::TextMuted : Theme::Color::BorderStrong)));
        if (state.clicked) {
            picked = static_cast<int>(index) + 1;
        }
    }

    // The dots take the focus together, and left and right turn the pages from there.
    (void)Widgets::interact(context, dots.expanded(size), size, "##pages");
    if (takeFocusRequest()) {
        Widgets::focusItem(context);
    }
    return picked;
}

void Carousel::turn(Context& context, int target, std::size_t count) {
    const auto pages = static_cast<int>(count);
    const int next = loop ? ((target - 1) % pages + pages) % pages + 1 : std::clamp(target, 1, pages);
    if (next == page) {
        return;
    }
    page = next;
    waited = 0.0F;
    context.emit(*this, "change", {{"page", page}});
}

} // namespace haylen::ui
