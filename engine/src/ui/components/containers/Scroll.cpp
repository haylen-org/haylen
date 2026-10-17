#include "ui/components/containers/Scroll.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include "haylen/core/JsonValidator.hpp"
#include "haylen/ui/Backend.hpp"
#include "haylen/ui/Context.hpp"
#include "haylen/ui/PropertyReader.hpp"
#include "ui/ImGuiConverter.hpp"

namespace haylen::ui {

void Scroll::readProperties(PropertyReader& reader) {
    std::string axis = horizontal ? "horizontal" : "vertical";
    reader.read("scrollbar", scrollbarShown);
    reader.read("axis", axis);
    reader.read("snap", snap);
    if (axis != "vertical" && axis != "horizontal") {
        reader.fail("axis", "must be \"vertical\" or \"horizontal\"");
    }
    horizontal = axis == "horizontal";
}

void Scroll::command(Context& context, std::string_view name, const core::Json& arguments) {
    if (name != "scrollTo") {
        Component::command(context, name, arguments);
        return;
    }
    if (!arguments.is_object() || arguments.contains("node") == arguments.contains("offset")) {
        throw std::invalid_argument("The \"scrollTo\" command of a \"scroll\" takes a table with either \"node\" or \"offset\".");
    }
    core::JsonValidator::requireKnownKeys(arguments, {"node", "offset", "align", "animated"}, "the \"scrollTo\" command of a \"scroll\"");
    PropertyReader reader(arguments, getKind());
    Request next;
    float offset = 0.0F;
    reader.read("node", next.node);
    reader.read("offset", offset, 0.0F, 1e9F);
    reader.readChoice<Align>("align", next.align, kAligns);
    reader.read("animated", next.animated);
    reader.finish();
    if (arguments.contains("offset")) {
        next.offset = offset;
    } else if (next.node.empty() || findNode(*this, next.node) == nullptr) {
        throw std::invalid_argument("The scroll \"" + getId() + "\" holds no node with the id \"" + next.node + "\".");
    }
    request = std::move(next);
}

const Component* Scroll::findNode(const Component& parent, std::string_view id) {
    for (const auto& child : parent.getChildren()) {
        if (child->getId() == id) {
            return child.get();
        }
        if (const Component* found = findNode(*child, id)) {
            return found;
        }
    }
    return nullptr;
}

// A horizontal scroll adds the lane of its bar to its height while its child is wider than it, so the child keeps the height it measures.
math::Vec2 Scroll::measureContent(Context& context, float availableWidth) {
    auto visible = getLayoutChildren();
    if (visible.empty()) {
        return {};
    }
    const math::Vec2 size = visible.front()->measure(context, horizontal ? CommonProperties::kUnbounded : availableWidth);
    const float lane = horizontal && scrollbarShown && size.x > availableWidth ? Scrollbar::getLane(context) : 0.0F;
    return {std::min(size.x, availableWidth), size.y + lane};
}

void Scroll::render(Context& context, const math::Rect& bounds) {
    // ImGui gives a child without a width or a height the rest of its window, so an empty scroll draws nothing instead.
    if (bounds.isEmpty()) {
        return;
    }
    auto visible = getLayoutChildren();
    Component* child = visible.empty() ? nullptr : visible.front();

    // The bar takes its lane while the child at the whole width is longer than the area, so the content never moves back and forth as the bar comes and goes.
    math::Vec2 size = child != nullptr ? child->measure(context, horizontal ? CommonProperties::kUnbounded : bounds.width) : math::Vec2{};
    const bool overflowing = scrollbarShown && (horizontal ? size.x > bounds.width : size.y > bounds.height);
    const math::Rect box = overflowing ? Scrollbar::getContentBox(context, bounds, bounds, horizontal) : bounds;
    if (overflowing && !horizontal) {
        size = child->measure(context, box.width);
    }

    ImGui::SetCursorScreenPos(ImGuiConverter::toImVec2(bounds.getMin()));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    if (horizontal) {
        flags |= ImGuiWindowFlags_HorizontalScrollbar;
    }
    if (context.getBackend().beginChild("##scroll", bounds.getSize(), ImGuiChildFlags_NavFlattened, flags)) {
        const float view = horizontal ? box.width : box.height;
        const ImVec2 start = ImGui::GetCursorScreenPos();
        const float shown = horizontal ? ImGui::GetScrollX() : ImGui::GetScrollY();
        scroller.setRange(std::max(0.0F, (horizontal ? size.x : size.y) - view), view);
        followWindow();

        // The wheel and the finger move the content along the axis, and the bar takes the pointer in its lane before the content does.
        const bool underPointer = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
        const Context::WheelScroll wheel = context.takeWheel(ImGui::GetCurrentWindow()->ID, bounds, horizontal, scroller.getOffset(), scroller.getMaximum(), underPointer);
        const bool wheeled = wheel.motion != Context::WheelScroll::Motion::None;
        scroller.follow(wheel, -1.0, context.getDeltaSeconds());
        if (wheeled) {
            request.reset();
        }
        followFinger(context, bounds, underPointer);
        if (overflowing) {
            if (const std::optional<double> offset = scrollbar.interact(context, bounds, horizontal, false, scroller.getOffset(), scroller.getMaximum(), view)) {
                scroller.jumpTo(*offset);
                request.reset();
            }
        }
        const bool touched = (underPointer && (wheeled || ImGui::IsMouseDown(ImGuiMouseButton_Left))) || ImGui::GetActiveID() != 0;
        idle = touched ? 0.0F : idle + context.getDeltaSeconds();
        scroller.update(context.getDeltaSeconds());

        // The ImGui window scrolls to the offset within the range in whole units, as ImGui keeps it, and the content draws at the offset itself, so it moves smoothly and stretches past an end.
        const double offset = scroller.getOffset();
        written = static_cast<float>(std::round(std::clamp(offset, 0.0, scroller.getMaximum())));
        (horizontal ? ImGui::GetCurrentWindow()->Scroll.x : ImGui::GetCurrentWindow()->Scroll.y) = *written;
        const float moved = shown - static_cast<float>(offset);
        const math::Vec2 shift = horizontal ? math::Vec2{moved, 0.0F} : math::Vec2{0.0F, moved};

        // The child lays out in the box the lane leaves, scrolled, and draws only inside it.
        points.clear();
        math::Vec2 origin;
        if (child != nullptr) {
            origin = math::Vec2{start.x, start.y} + (box.getMin() - bounds.getMin()) + shift;
            const math::Rect area = place(context, *child, size, {origin.x, origin.y, box.width, box.height});
            ImGui::PushClipRect(ImGuiConverter::toImVec2(box.getMin()), ImGuiConverter::toImVec2(box.getMax()), true);
            child->draw(context, area);
            ImGui::PopClipRect();
            ImGui::SetCursorScreenPos(start);
            const math::Vec2 end = math::Vec2::max(area.getMax(), origin + box.getSize()) - shift;
            ImGui::Dummy({end.x - start.x, end.y - start.y});

            // The items of the child are the places snapping settles on, measured from the start of the content.
            for (const auto& item : child->getChildren()) {
                if (item->getCommon().visible && !item->getBounds().isEmpty()) {
                    points.push_back(horizontal ? item->getBounds().x - origin.x : item->getBounds().y - origin.y);
                }
            }
        }

        if (request) {
            const std::optional<double> target = findTarget(*request, origin, view);
            if (!target) {
                request.reset();
            } else if (!request->animated) {
                scroller.jumpTo(*target);
                request.reset();
            } else if (scroller.isResting() && std::fabs(*target - scroller.getOffset()) <= kSettleDistance) {
                request.reset();
            } else {
                scroller.animateTo(*target);
            }
            idle = 0.0F;
        }
        settle();
        if (overflowing) {
            scrollbar.draw(context, 1.0F);
        }
    }
    ImGui::EndChild();
}

// The child spans the scrolling axis at its measured length and stretches across the other axis within its size bounds, or sits there at the alignment it sets.
math::Rect Scroll::place(Context& context, Component& child, math::Vec2 size, const math::Rect& view) const {
    const Alignment alignment = child.getCommon().align.value_or(Alignment::Stretch);
    if (horizontal) {
        const float height = alignment == Alignment::Stretch ? std::min(child.clampHeight(view.height), view.height) : std::min(size.y, view.height);
        return {view.x, std::floor(align(alignment, view.y, view.height, height)), size.x, height};
    }
    const float width = alignment == Alignment::Stretch ? std::min(child.clampWidth(view.width), view.width) : std::min(size.x, view.width);
    return {std::floor(context.alignHorizontally(alignment, view.x, view.width, width)), view.y, width, size.y};
}

void Scroll::followWindow() {
    const ImGuiWindow& window = *ImGui::GetCurrentWindow();
    const float shown = horizontal ? window.Scroll.x : window.Scroll.y;
    const float limit = horizontal ? window.ScrollMax.x : window.ScrollMax.y;
    if (written && !dragging && shown != std::min(*written, limit)) {
        scroller.jumpTo(shown);
        request.reset();
    }
}

void Scroll::followFinger(Context& context, const math::Rect& bounds, bool underPointer) {
    const ImGuiIO& io = ImGui::GetIO();
    const ImGuiContext& state = *GImGui;
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        pressed = underPointer;
        dragging = false;
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        if (dragging) {
            scroller.release();
        }
        pressed = false;
        dragging = false;
        return;
    }
    if (!pressed || io.MouseSource != ImGuiMouseSource_TouchScreen) {
        return;
    }

    // A text field being edited keeps the finger.
    if (!dragging) {
        if (scroller.getMaximum() <= 0.0 || !context.takeDrag(ImGui::GetCurrentWindow()->ID, bounds, horizontal)) {
            return;
        }
        if (state.ActiveId != 0 && io.WantTextInput) {
            pressed = false;
            return;
        }
        if (state.ActiveId != 0) {
            ImGui::ClearActiveID();
        }
        dragging = true;
        request.reset();
    }
    scroller.drag(-static_cast<double>(horizontal ? io.MouseDelta.x : io.MouseDelta.y), context.getDeltaSeconds());
}

// A node scrolls only as far as it needs to show whole with `nearest`, and aligns its start, middle or end with the view otherwise.
std::optional<double> Scroll::findTarget(const Request& value, math::Vec2 origin, double view) const {
    if (value.offset) {
        return std::clamp(static_cast<double>(*value.offset), 0.0, scroller.getMaximum());
    }
    const Component* node = findNode(*this, value.node);
    if (node == nullptr || !node->getCommon().visible) {
        return std::nullopt;
    }
    const math::Rect area = node->getBounds();
    const double position = horizontal ? area.x - origin.x : area.y - origin.y;
    const double length = horizontal ? area.width : area.height;
    const double current = scroller.getOffset();
    double target = current;
    switch (value.align) {
    case Align::Start:
        target = position;
        break;
    case Align::Center:
        target = position + (length - view) * 0.5;
        break;
    case Align::End:
        target = position + length - view;
        break;
    case Align::Nearest:
        if (position < current || length > view) {
            target = position;
        } else if (position + length > current + view) {
            target = position + length - view;
        }
        break;
    }
    return std::clamp(target, 0.0, scroller.getMaximum());
}

// Once the player lets go and the content rests, it glides to the item of the child nearest to where it stopped.
void Scroll::settle() {
    if (!snap || points.empty() || dragging || request || !scroller.isResting() || idle < kSnapDelay) {
        return;
    }
    const double position = scroller.getOffset();
    double target = std::clamp(static_cast<double>(points.front()), 0.0, scroller.getMaximum());
    for (const float point : points) {
        const double candidate = std::clamp(static_cast<double>(point), 0.0, scroller.getMaximum());
        if (std::fabs(candidate - position) < std::fabs(target - position)) {
            target = candidate;
        }
    }
    if (std::fabs(target - position) > kSettleDistance) {
        scroller.animateTo(target);
    }
}

} // namespace haylen::ui
