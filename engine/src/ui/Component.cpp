#include "haylen/ui/Component.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <utility>

#include <imgui.h>

#include "haylen/math/Insets.hpp"
#include "haylen/ui/Backend.hpp"
#include "haylen/ui/Context.hpp"
#include "haylen/ui/FocusNavigator.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/Surfaces.hpp"
#include "ui/Typography.hpp"

namespace haylen::ui {

float Component::align(Alignment alignment, float start, float available, float size) noexcept {
    switch (alignment) {
    case Alignment::Center:
        return start + (available - size) * 0.5F;
    case Alignment::End:
        return start + available - size;
    default:
        return start;
    }
}

void Component::readCommon(PropertyReader& reader) {
    reader.read("visible", common.visible);
    reader.read("enabled", common.enabled);
    reader.read("tooltip", common.tooltip);
    reader.read("grow", common.grow, 0.0F, 1000.0F);
    reader.readLength("width", common.width);
    reader.readLength("height", common.height);
    reader.read("minWidth", common.minWidth, 0.0F);
    reader.read("maxWidth", common.maxWidth, 0.0F);
    reader.read("minHeight", common.minHeight, 0.0F);
    reader.read("maxHeight", common.maxHeight, 0.0F);

    Alignment alignment = getAlignment();
    if (reader.has("align")) {
        reader.readChoice<Alignment>("align", alignment, kAlignments);
        common.align = alignment;
    }

    std::string anchor;
    reader.read("anchor", anchor);
    if (anchor == "none") {
        common.anchor.reset();
    } else if (!anchor.empty()) {
        common.anchor = Anchor::fromName(anchor);
        if (!common.anchor) {
            reader.fail("anchor", "must be none or an anchor name such as topLeft, center or stretchHorizontal");
        }
    }
    reader.readChoice<Anchor::Area>("anchorTo", common.anchorArea, kAnchorAreas);
    reader.read("margin", common.margin);
    reader.readChoice<std::optional<text::Direction>>("direction", common.direction, kDirections);
    reader.read("language", common.language);
    readFocus(reader);
}

// A node that sets its direction or language measures and draws itself and its children in them.
bool Component::pushWriting(Context& context) const {
    if (!common.direction && common.language.empty()) {
        return false;
    }
    context.pushWriting(common.direction, common.language.empty() ? std::nullopt : std::optional<std::string>(common.language));
    return true;
}

void Component::readFocus(PropertyReader& reader) {
    reader.read("focusable", common.focusable);
    reader.read("autofocus", common.autofocus);
    reader.read("focusScope", common.focusScope);
    reader.readChoice<FocusWrap>("focusWrap", common.focusWrap, kFocusWraps);
    for (std::size_t index = 0; index < kNeighborKeys.size(); ++index) {
        reader.read(kNeighborKeys[index], common.focusNeighbors[index]);
    }
}

void Component::apply(const core::Json& properties) {
    PropertyReader reader(properties, getKind());
    readCommon(reader);
    readProperties(reader);
    reader.finish();
    measuredFrame = 0;
}

float Component::getBoundedWidth(float width) const noexcept {
    return std::clamp(width, common.minWidth, std::max(common.minWidth, common.maxWidth));
}

float Component::getBoundedHeight(float height) const noexcept {
    return std::clamp(height, common.minHeight, std::max(common.minHeight, common.maxHeight));
}

math::Vec2 Component::measure(Context& context, float availableWidth) {
    if (!common.visible || isFloating()) {
        return {};
    }

    // Parents measure a child before drawing it and the child is measured again while it draws, so one answer per frame and width is kept.
    if (measuredFrame == context.getFrame() && measuredWidth == availableWidth) {
        return measuredSize;
    }
    const float offered = common.width.value_or(getBoundedWidth(availableWidth));
    const bool writing = pushWriting(context);
    const math::Vec2 content = measureContent(context, offered);
    if (writing) {
        context.popWriting();
    }
    measuredSize = {common.width.value_or(getBoundedWidth(content.x)), common.height.value_or(getBoundedHeight(content.y))};
    measuredFrame = context.getFrame();
    measuredWidth = availableWidth;
    return measuredSize;
}

void Component::draw(Context& context, const math::Rect& layout) {
    if (!common.visible) {
        return;
    }
    const math::Rect bounds = layout.translated(transform->offset);
    const bool appeared = drawnFrame == 0 || drawnFrame + 1 != context.getFrame();
    drawnBounds = bounds;
    drawnFrame = context.getFrame();

    ImGui::PushID(this);
    drawId = ImGui::GetID("##node");
    FocusNavigator& focus = context.getFocus();
    focus.enter(*this, drawId, bounds);
    for (const std::string_view notice : focus.takeNotices(drawId)) {
        context.emit(*this, std::string(notice));
    }

    // Autofocus takes the focus when the node appears and the focus is elsewhere, such as when its document mounts or its dialog opens.
    if (common.autofocus && appeared && isFocusable() && !focus.hasFocusHere()) {
        focusRequested = true;
    }

    if (!common.enabled) {
        ImGui::BeginDisabled();
    }
    const bool writing = pushWriting(context);
    // The context carries the transform to the draws of the node that go around the ImGui vertices, such as rich text.
    const bool reshaping = transform->isReshaping();
    const int firstVertex = ImGui::GetWindowDrawList()->VtxBuffer.Size;
    if (reshaping) {
        context.pushReshape(*transform, bounds.getCenter());
    }
    render(context, bounds);
    if (reshaping) {
        context.popReshape();
        reshape(firstVertex, *transform, bounds.getCenter());
    }
    drawDetachedChildren(context, bounds);
    if (!common.enabled) {
        ImGui::EndDisabled();
    }
    drawTooltip(context, bounds);
    if (writing) {
        context.popWriting();
    }
    focus.leave();
    ImGui::PopID();
}

math::Rect Component::getAnchoredBounds(Context& context) {
    const Backend& backend = context.getBackend();
    const math::Rect area = (common.anchorArea == Anchor::Area::Safe ? backend.getSafeRect() : backend.getDisplayRect()).inset(common.margin);
    return common.anchor.value_or(Anchor{}).place(measure(context, area.width), area);
}

void Component::reshape(int firstVertex, const Transform& shape, math::Vec2 center) {
    ImDrawList& list = *ImGui::GetWindowDrawList();
    const std::array<std::pair<unsigned, float>, 4> channels{{
        {IM_COL32_R_SHIFT, shape.tint.r},
        {IM_COL32_G_SHIFT, shape.tint.g},
        {IM_COL32_B_SHIFT, shape.tint.b},
        {IM_COL32_A_SHIFT, shape.tint.a * shape.opacity},
    }};
    for (int index = firstVertex; index < list.VtxBuffer.Size; ++index) {
        ImDrawVert& vertex = list.VtxBuffer[index];
        vertex.pos = {center.x + (vertex.pos.x - center.x) * shape.scale.x, center.y + (vertex.pos.y - center.y) * shape.scale.y};

        ImU32 color = 0;
        for (const auto& [shift, factor] : channels) {
            const float value = static_cast<float>((vertex.col >> shift) & 0xFFU) * std::clamp(factor, 0.0F, 1.0F);
            color |= static_cast<ImU32>(std::lround(value)) << shift;
        }
        vertex.col = color;
    }
}

// Children outside the layout draw after it: anchored ones where their anchor places them, and floating ones over everything, from their own position.
void Component::drawDetachedChildren(Context& context, const math::Rect& bounds) {
    for (const auto& child : children) {
        if (!child->common.visible) {
            continue;
        }
        if (child->common.anchor) {
            child->draw(context, child->getAnchoredBounds(context));
        } else if (child->isFloating()) {
            child->draw(context, {bounds.x, bounds.y, 0.0F, 0.0F});
        }
    }
}

std::optional<FocusDirection> Component::takeFocusDirection(Context& context) const {
    return context.getFocus().takeDirection(drawId);
}

void Component::drawTooltip(Context& context, const math::Rect& bounds) {
    const bool hovered = !common.tooltip.isEmpty() && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && bounds.contains(math::Vec2{ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y});
    if (!hovered) {
        hoverStarted = -1.0;
        return;
    }
    if (hoverStarted < 0.0) {
        hoverStarted = context.getTime();
    }
    if (context.getTime() - hoverStarted < kTooltipDelaySeconds) {
        return;
    }

    // The tooltip window takes the size of the text, and the theme paints the tooltip surface behind it, keeping the text inside the padding of a surface image.
    const math::Insets padding = Surfaces::getPadding(context, Theme::Surface::Tooltip);
    const ImVec2 spacing = ImGui::GetStyle().WindowPadding;
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.0F, 0.0F, 0.0F, 0.0F));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0.0F);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {spacing.x + std::max(padding.left, padding.right), spacing.y + std::max(padding.top, padding.bottom)});
    if (ImGui::BeginTooltip()) {
        const ImVec2 position = ImGui::GetWindowPos();
        const ImVec2 size = ImGui::GetWindowSize();
        Surfaces::draw(context, Theme::Surface::Tooltip, {position.x, position.y, size.x, size.y}, context.getColor(Theme::Color::Tooltip));
        const std::string text = context.getText(common.tooltip);
        const math::Vec2 measured = Typography::measureParagraph(context, Theme::Font::Caption, text, context.getMetric(Theme::Metric::TooltipWidth));
        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        Typography::drawParagraph(context, Theme::Font::Caption, {cursor.x, cursor.y, measured.x, measured.y}, context.getColor(Theme::Color::OnTooltip), text, Alignment::Start);
        ImGui::Dummy(ImGuiConverter::toImVec2(measured));
        ImGui::EndTooltip();
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

void Component::command(Context&, std::string_view name, const core::Json& arguments) {
    if (name != "focus" || !isFocusable()) {
        throw std::invalid_argument("The component kind '" + std::string(getKind()) + "' does not answer the command '" + std::string(name) + "'.");
    }
    if (!arguments.is_null() && !(arguments.is_object() && arguments.empty())) {
        throw std::invalid_argument("The focus command takes no arguments.");
    }
    focusRequested = true;
}

void Component::noticeStoppedDrawing(Context& context) {
    if (drawnFrame != 0 && drawnFrame + 1 == context.getFrame()) {
        drawingStopped(context);
    }
    for (const auto& child : children) {
        child->noticeStoppedDrawing(context);
    }
}

std::vector<Component*> Component::getLayoutChildren() const {
    std::vector<Component*> placed;
    for (const auto& child : children) {
        if (child->getCommon().visible && !child->getCommon().anchor && !child->isFloating()) {
            placed.push_back(child.get());
        }
    }
    return placed;
}

} // namespace haylen::ui
