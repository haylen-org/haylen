#include "ui/components/overlays/Window.hpp"

#include <algorithm>
#include <format>
#include <string>
#include <utility>

#include <imgui.h>

#include "haylen/core/JsonNumber.hpp"
#include "haylen/ui/Backend.hpp"
#include "haylen/ui/Context.hpp"
#include "haylen/ui/FocusNavigator.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/Surfaces.hpp"
#include "ui/Typography.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

void Window::readMore(PropertyReader& reader) {
    reader.read("title", title);
    reader.read("open", open);
    reader.read("closable", closable);
    reader.read("movable", movable);
    if (!reader.has("x") && !reader.has("y")) {
        return;
    }

    // A coordinate given alone keeps the other one where the window is.
    if (position) {
        requestedX = position->x;
        requestedY = position->y;
    }
    if (reader.has("x")) {
        float x = 0.0F;
        reader.read("x", x);
        requestedX = x;
    }
    if (reader.has("y")) {
        float y = 0.0F;
        reader.read("y", y);
        requestedY = y;
    }
    position.reset();
}

math::Insets Window::getPadding(Context& context) const {
    const math::Insets own = Linear::getPadding(context);
    const math::Insets image = Surfaces::getPadding(context, Theme::Surface::Window);
    const float panel = own == math::Insets{} ? context.getMetric(Theme::Metric::PanelPadding) : 0.0F;
    return {own.left + image.left + panel, own.top + panel, own.right + image.right + panel, own.bottom + image.bottom + panel};
}

math::Vec2 Window::measureContent(Context&, float) {
    return {};
}

math::Vec2 Window::getSize(Context& context) {
    const math::Rect display = context.getBackend().getDisplayRect();
    const float header = context.getMetric(Theme::Metric::WindowTitleHeight);
    const float width = std::min(getCommon().width.value_or(Linear::measureContent(context, display.width).x), display.width);
    const float height = std::min(getCommon().height.value_or(header + Linear::measureContent(context, width).y), display.height);
    return {width, height};
}

void Window::render(Context& context, const math::Rect&) {
    if (!open) {
        return;
    }

    // The window starts where the app asked or in the middle of the safe area, and its title bar never leaves the screen.
    const math::Vec2 size = getSize(context);
    const math::Rect display = context.getBackend().getDisplayRect();
    const float header = context.getMetric(Theme::Metric::WindowTitleHeight);
    const math::Vec2 centered = context.getBackend().getSafeRect().getCenter() - size * 0.5F;
    const math::Vec2 start = position.value_or(math::Vec2{requestedX.value_or(centered.x), requestedY.value_or(centered.y)});
    position = math::Vec2{std::clamp(start.x, display.x, std::max(display.x, display.getRight() - size.x)), std::clamp(start.y, display.y, std::max(display.y, display.getBottom() - header))};
    setBounds({position->x, position->y, size.x, size.y});

    ImGui::SetNextWindowPos(ImGuiConverter::toImVec2(*position));
    ImGui::SetNextWindowSize(ImGuiConverter::toImVec2(size));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0F, 0.0F});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoFocusOnAppearing;
    const bool shown = ImGui::Begin(std::format("##window{}", static_cast<const void*>(this)).c_str(), nullptr, flags);
    ImGui::PopStyleVar(2);
    if (shown) {
        const math::Rect frame{position->x, position->y, size.x, size.y};
        Surfaces::draw(context, Theme::Surface::Window, frame, context.getColor(Theme::Color::Panel), context.getColor(Theme::Color::Border));
        drawTitle(context, {frame.x, frame.y, frame.width, header});
        Linear::render(context, {frame.x, frame.y + header, frame.width, std::max(0.0F, frame.height - header)});
        if (open && closable && context.getFocus().answerCancel(ImGui::GetCurrentWindow())) {
            close(context);
        }
    }
    ImGui::End();
}

void Window::drawTitle(Context& context, const math::Rect& bar) {
    const float radius = context.getMetric(Theme::Metric::ControlRadius);
    const float side = closable ? bar.height : 0.0F;
    const math::Rect grip = context.mirror({bar.x, bar.y, bar.width - side, bar.height}, bar);
    ImDrawList& list = *ImGui::GetWindowDrawList();
    list.AddRectFilled(ImGuiConverter::toImVec2(bar.getMin()), ImGuiConverter::toImVec2(bar.getMax()), ImGuiConverter::toImU32(context.getColor(Theme::Color::Raised)), radius, ImDrawFlags_RoundCornersTop);
    list.AddLine({bar.x, bar.getBottom()}, {bar.getRight(), bar.getBottom()}, ImGuiConverter::toImU32(context.getColor(Theme::Color::Border)), context.getMetric(Theme::Metric::BorderWidth));
    const float padding = context.getMetric(Theme::Metric::ControlPaddingX);
    Typography::drawAligned(context, Theme::Font::Button, {grip.x + padding, grip.y, std::max(0.0F, grip.width - padding * 2.0F), grip.height}, context.getColor(Theme::Color::Text), context.getText(title), Alignment::Start);

    if (closable) {
        const math::Rect cross = context.mirror({bar.getRight() - side, bar.y, side, side}, bar).expanded(-side * 0.15F);
        const Widgets::Interaction state = Widgets::interact(context, cross, radius, "##close");
        if (state.hovered) {
            list.AddRectFilled(ImGuiConverter::toImVec2(cross.getMin()), ImGuiConverter::toImVec2(cross.getMax()), ImGuiConverter::toImU32(context.getColor(Theme::Color::Hover)), radius);
        }
        const math::Vec2 center = cross.getCenter();
        const float arm = cross.width * 0.2F;
        const ImU32 color = ImGuiConverter::toImU32(context.getColor(state.hovered ? Theme::Color::DangerText : Theme::Color::TextMuted));
        list.AddLine({center.x - arm, center.y - arm}, {center.x + arm, center.y + arm}, color, 2.0F);
        list.AddLine({center.x - arm, center.y + arm}, {center.x + arm, center.y - arm}, color, 2.0F);
        if (state.clicked) {
            close(context);
        }
    }
    if (!movable) {
        return;
    }

    // The title bar moves the window while the pointer holds it, and the new place is reported once it lets go.
    const Widgets::Interaction drag = Widgets::interact(context, grip, 0.0F, "##title", ImGuiButtonFlags_NoNavFocus);
    if (drag.held) {
        const ImVec2 delta = ImGui::GetIO().MouseDelta;
        position = *position + math::Vec2{delta.x, delta.y};
        dragging = true;
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    } else if (std::exchange(dragging, false)) {
        context.emit(*this, "move", {{"x", core::JsonNumber::fromFloat(position->x)}, {"y", core::JsonNumber::fromFloat(position->y)}});
    }
}

void Window::close(Context& context) {
    open = false;
    context.emit(*this, "close");
}

} // namespace haylen::ui
