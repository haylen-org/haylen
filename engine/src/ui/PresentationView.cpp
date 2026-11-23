#include "ui/PresentationView.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include <imgui_internal.h>

#include "haylen/ui/AccessibilityTree.hpp"
#include "haylen/ui/Backend.hpp"
#include "haylen/ui/Context.hpp"
#include "haylen/ui/Gui.hpp"
#include "haylen/ui/ImGuiConverter.hpp"
#include "haylen/ui/Screen.hpp"
#include "haylen/ui/Surfaces.hpp"
#include "ui/ScrollArbiter.hpp"

namespace haylen::ui {

PresentationView::PresentationView(Backend& drawing, Context& drawingContext) : backend(drawing), context(drawingContext) {}

// The anchor of a popover counts only while its node drew in this frame or the last one, so a popover whose control went away floats in the middle.
Presentation::Frame PresentationView::prepare(Presentation& presentation, Gui& gui) {
    Presentation::Frame frame{
        .display = backend.getLiftedDisplayRect(),
        .safe = backend.getLiftedSafeRect(),
        .pointsPerUnit = backend.getPointsPerUnit(),
        .radius = context.getMetric(Theme::Metric::PanelRadius),
        .rightToLeft = context.isRightToLeft(),
    };

    if (const std::optional<Region>& anchor = presentation.getOptions().anchor; anchor && anchor->gui) {
        const Component* node = anchor->gui->find(anchor->node);
        if (node != nullptr && node->getDrawnFrame() + 1 >= context.getFrame() && !node->getBounds().isEmpty()) {
            frame.anchor = node->getBounds();
        }
    }

    const float width = presentation.getAreaWidth(frame);
    frame.contentHeight = gui.measure(context, width).y;
    presentation.lastFrame = frame;
    return frame;
}

math::Vec2 PresentationView::getOffset(const Presentation::Layout& layout, const Navigation::Look& look) const {
    const math::Rect display = backend.getLiftedDisplayRect();
    switch (look.effect) {
    case NavigationTransition::Effect::Slide: {
        const float width = layout.surface.isEmpty() ? layout.area.width : layout.surface.width;
        const float toward = context.isRightToLeft() ? 1.0F : -1.0F;
        return {look.distance * toward * width, 0.0F};
    }
    case NavigationTransition::Effect::Rise: {
        const float top = layout.surface.isEmpty() ? layout.area.y : std::min(layout.area.y, layout.surface.y);
        return {0.0F, look.distance * (display.getBottom() - top)};
    }
    case NavigationTransition::Effect::None:
    case NavigationTransition::Effect::Fade:
        break;
    }

    return {};
}

void PresentationView::drawChrome(Presentation& presentation, const Presentation::Layout& layout, const Navigation::Look& look, math::Vec2 offset, bool interactive) {
    const float shown = 1.0F - look.distance;
    const Presentation::Style style = presentation.getStyle();
    const bool floating = style == Presentation::Style::Popover && !layout.compact;
    if (presentation.isModal() && style != Presentation::Style::FullScreen && !floating) {
        drawBackdrop(shown);
    }

    drawSurface(presentation, layout, look, offset);
    if (!interactive) {
        return;
    }

    const math::Rect surface = layout.surface.translated(offset);
    backend.blockPointer(presentation.isModal() ? backend.getLiftedDisplayRect() : surface);
    if (look.inert) {
        return;
    }

    // The surface takes the presses that no control of the screen takes, so a drag on its empty parts moves it, and the grabber takes its own press, which cycles the detents.
    const bool card = presentation.isCard(*presentation.lastFrame);
    const bool sheet = style == Presentation::Style::Sheet || (style == Presentation::Style::Popover && layout.compact);
    const bool movable = presentation.getOptions().detents.size() > 1 || presentation.isDismissible();
    ImGui::PushID(&presentation);
    if ((sheet || card || style == Presentation::Style::Drawer) && movable && !surface.isEmpty()) {
        ImGui::SetCursorScreenPos(ImGuiConverter::toImVec2(surface.getMin()));
        ImGui::SetNextItemAllowOverlap();
        static_cast<void>(ImGui::InvisibleButton("##surface", ImGuiConverter::toImVec2(surface.getSize()), ImGuiButtonFlags_NoNavFocus));
        followDrag(presentation, ImGui::GetItemID());
    }

    if (!layout.grabber.isEmpty()) {
        const math::Rect grabber = layout.grabber.translated(offset);
        const math::Rect reach = math::Rect::fromCenter(grabber.getCenter(), {grabber.width * 2.0F, Presentation::kGrabberBand});
        ImGui::SetCursorScreenPos(ImGuiConverter::toImVec2(reach.getMin()));
        const bool clicked = ImGui::InvisibleButton("##grabber", ImGuiConverter::toImVec2(reach.getSize()));
        const ImGuiID item = ImGui::GetItemID();
        AccessibilityTree& tree = context.getAccessibility();
        tree.addButton(context, item, reach, "Sheet grabber");
        const bool pressed = clicked || tree.takePress(item);
        if (pressed && !presentation.dragging) {
            requests.cycled = presentation.weak_from_this();
        }

        followDrag(presentation, item);
    }

    ImGui::PopID();
    followBackdrop(presentation, layout.surface.isEmpty() ? layout.surface : surface, look);
}

void PresentationView::drawBackdrop(float shown) {
    const math::Rect display = backend.getLiftedDisplayRect();
    const math::Color dim = context.getColor(Theme::Color::Overlay);
    ImDrawList& list = *ImGui::GetWindowDrawList();
    list.AddRectFilled(ImGuiConverter::toImVec2(display.getMin()), ImGuiConverter::toImVec2(display.getMax()), ImGuiConverter::toImU32(dim.withAlpha(dim.a * shown)));
}

// The surface moves and fades with its screen, a floating card or popover casts the shadow of the theme, and a sheet shows its grabber in the band along its top.
void PresentationView::drawSurface(const Presentation& presentation, const Presentation::Layout& layout, const Navigation::Look& look, math::Vec2 offset) {
    if (layout.surface.isEmpty()) {
        return;
    }

    const Presentation::Style style = presentation.getStyle();
    const Theme::Color role = style == Presentation::Style::FullScreen ? Theme::Color::Background : Theme::Color::Panel;
    const math::Color color = presentation.getOptions().color.value_or(context.getColor(role));
    const float alpha = look.effect == NavigationTransition::Effect::Fade ? 1.0F - look.distance : 1.0F;
    const math::Rect surface = layout.surface.translated(offset);
    const bool card = presentation.isCard(*presentation.lastFrame);
    if (card || (style == Presentation::Style::Popover && !layout.compact)) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * alpha);
        Surfaces::drawShadow(context, surface, layout.radii[0]);
        ImGui::PopStyleVar();
    }

    backend.addShape({.bounds = surface, .radii = layout.radii, .color = color.withAlpha(color.a * alpha)});
    if (layout.arrow) {
        Presentation::Layout moved = layout;
        moved.arrow = *layout.arrow + offset;
        drawArrow(moved, color, color.a * alpha);
    }

    if (!layout.grabber.isEmpty()) {
        const math::Color ink = context.getColor(Theme::Color::TextMuted);
        const math::Rect grabber = layout.grabber.translated(offset);
        Surfaces::fill(context, grabber, ink.withAlpha(ink.a * 0.5F * alpha), grabber.height * 0.5F);
    }
}

// The arrow is a triangle from the edge of the popover to the middle of its anchor, overlapping the surface by a unit so no seam shows between them.
void PresentationView::drawArrow(const Presentation::Layout& layout, math::Color color, float alpha) {
    const math::Vec2 tip = *layout.arrow;
    const float size = Presentation::kArrowSize;
    const float toward = layout.arrowUp ? 1.0F : -1.0F;
    const float base = tip.y + toward * (size + 1.0F);
    ImDrawList& list = *ImGui::GetWindowDrawList();
    list.AddTriangleFilled({tip.x - size - 1.0F, base}, {tip.x, tip.y}, {tip.x + size + 1.0F, base}, ImGuiConverter::toImU32(color.withAlpha(alpha)));
}

float PresentationView::getDragDistance(const Presentation& presentation, const ImVec2& dragged) const {
    if (presentation.getStyle() != Presentation::Style::Drawer) {
        return dragged.y;
    }

    return context.isRightToLeft() ? dragged.x : -dragged.x;
}

// A press drags the surface once it travels past the slop along the axis the surface closes on, from where the surface showed, and its speed follows the last moments of the motion. The drag ends when the press lets go, which the next update settles.
void PresentationView::followDrag(Presentation& presentation, ImGuiID item) {
    if (ImGui::IsItemActive()) {
        const ImVec2 dragged = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0F);
        const float along = getDragDistance(presentation, dragged);
        const float across = presentation.getStyle() == Presentation::Style::Drawer ? dragged.y : dragged.x;
        if (!presentation.dragging) {
            if (std::fabs(along) < ScrollArbiter::kDragSlop || std::fabs(along) <= std::fabs(across)) {
                return;
            }

            presentation.dragStart = presentation.offset;
        }

        const float distance = presentation.dragStart + along;
        const float seconds = std::max(context.getDeltaSeconds(), 1.0e-4F);
        const float motion = (distance - presentation.offset) / seconds;
        const float weight = std::min(1.0F, seconds / kSpeedWindow);
        presentation.drag(distance, presentation.speed + (motion - presentation.speed) * weight);
        context.keepDrag(item);
        return;
    }

    if (presentation.dragging && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        requests.released = presentation.weak_from_this();
    }
}

// A tap outside the surface, or outside the root of a screen that draws its own card, dismisses a modal presentation in the next update, unless a control under the pointer took it.
void PresentationView::followBackdrop(Presentation& presentation, const math::Rect& surface, const Navigation::Look& look) {
    if (!presentation.isModal() || !presentation.isDismissible() || look.inert || !ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::GetCurrentContext()->HoveredIdPreviousFrame != 0) {
        return;
    }

    const ImVec2 position = ImGui::GetIO().MousePos;
    const math::Vec2 pointer{position.x, position.y};
    const std::shared_ptr<Gui>& gui = presentation.getScreen()->getGui();
    const math::Rect kept = surface.isEmpty() && gui ? gui->getRoot().getBounds() : surface;
    if (!kept.contains(pointer)) {
        requests.tapped = presentation.weak_from_this();
    }
}

float PresentationView::getBehindScale(const Presentation& presentation, const Navigation::Look& look) const {
    if (!presentation.lastFrame) {
        return 1.0F;
    }

    const float scale = presentation.getLayout(*presentation.lastFrame).behindScale;
    return 1.0F - (1.0F - scale) * (1.0F - look.distance);
}

PresentationView::Requests PresentationView::takeRequests() noexcept {
    return std::exchange(requests, {});
}

} // namespace haylen::ui
