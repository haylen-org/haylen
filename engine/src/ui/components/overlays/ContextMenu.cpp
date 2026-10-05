#include "ui/components/overlays/ContextMenu.hpp"

#include <stdexcept>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include "haylen/ui/Context.hpp"
#include "haylen/ui/FocusNavigator.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/components/PopupList.hpp"

namespace haylen::ui {

void ContextMenu::readProperties(PropertyReader& reader) {
    ChoiceItem::readList(reader, "items", items);
}

math::Vec2 ContextMenu::measureContent(Context& context, float availableWidth) {
    auto visible = getLayoutChildren();
    return visible.empty() ? math::Vec2{} : visible.front()->measure(context, availableWidth);
}

void ContextMenu::command(Context& context, std::string_view name, const core::Json& arguments) {
    if (name != "open") {
        Component::command(context, name, arguments);
        return;
    }
    if (!arguments.is_null() && !(arguments.is_object() && arguments.empty())) {
        throw std::invalid_argument("The \"open\" command takes no arguments.");
    }
    openRequested = true;
}

void ContextMenu::render(Context& context, const math::Rect& bounds) {
    if (auto visible = getLayoutChildren(); !visible.empty()) {
        visible.front()->draw(context, bounds);
    }

    if (const std::optional<math::Vec2> opening = findOpening(context, bounds)) {
        ImGui::OpenPopup("##context");
        ImGui::SetNextWindowPos(ImGuiConverter::toImVec2(*opening), ImGuiCond_Always, {context.isRightToLeft() ? 1.0F : 0.0F, 0.0F});
    }
    if (const std::optional<std::string> picked = PopupList::draw(context, "##context", items, {}, 0.0F)) {
        context.emit(*this, "select", {{"item", *picked}});
    }
}

// Returns where the menu opens this frame: at the pointer for a right click or a long press, below the focused control for `uiMenu`, and below the child for the `open` command. The menu hangs from that point toward the end of the UI.
std::optional<math::Vec2> ContextMenu::findOpening(Context& context, const math::Rect& bounds) {
    const ImGuiIO& io = ImGui::GetIO();
    const math::Vec2 pointer{io.MousePos.x, io.MousePos.y};
    const bool over = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && bounds.contains(pointer);
    if (std::exchange(openRequested, false)) {
        return math::Vec2{context.isRightToLeft() ? bounds.getRight() : bounds.x, bounds.getBottom()};
    }
    if (over && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
        return pointer;
    }

    // A finger held still opens the menu once, and the control under it does not take the press as a tap.
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        pressHandled = false;
    } else if (!pressHandled && over && io.MouseSource == ImGuiMouseSource_TouchScreen && io.MouseDownDuration[0] >= context.getMetric(Theme::Metric::LongPressDuration) && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        pressHandled = true;
        ImGui::ClearActiveID();
        return pointer;
    }

    if (context.getFocus().isMenuPressed()) {
        if (const std::optional<math::Rect> focused = context.getFocus().findFocusIn(bounds)) {
            return math::Vec2{context.isRightToLeft() ? focused->getRight() : focused->x, focused->getBottom()};
        }
    }
    return std::nullopt;
}

void ContextMenu::drawingStopped(Context&) {
    openRequested = false;
    pressHandled = false;
}

} // namespace haylen::ui
