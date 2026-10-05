#include "haylen/ui/Component.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <utility>

#include <imgui.h>
#include <imgui_internal.h>

#include "haylen/input/Controls.hpp"
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

std::optional<Component::Notice> Component::noticeFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kNoticeNames, name);
    if (found == kNoticeNames.end()) {
        return std::nullopt;
    }
    return static_cast<Notice>(found - kNoticeNames.begin());
}

void Component::setListening(Notice notice, bool value) noexcept {
    listening = value ? listening | toBit(notice) : listening & static_cast<std::uint16_t>(~toBit(notice));
}

void Component::readCommon(PropertyReader& reader) {
    reader.read("visible", common.visible);
    reader.read("enabled", common.enabled);
    reader.read("tooltip", common.tooltip);
    reader.read("grow", common.grow, 0.0F, 1000.0F);
    reader.readLength("width", common.width);
    reader.readLength("height", common.height);
    if (reader.has("aspectRatio")) {
        float ratio = 1.0F;
        reader.read("aspectRatio", ratio, kMinAspectRatio, kMaxAspectRatio);
        common.aspectRatio = ratio;
    }
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
            reader.fail("anchor", "must be \"none\" or an anchor name such as \"topLeft\", \"center\" or \"stretchHorizontal\"");
        }
    }
    reader.readChoice<Anchor::Area>("anchorTo", common.anchorArea, kAnchorAreas);
    reader.read("margin", common.margin);
    reader.readChoice<std::optional<text::Direction>>("direction", common.direction, kDirections);
    reader.read("language", common.language);
    reader.read("theme", common.theme);
    if (const core::Json* style = reader.take("style")) {
        common.style = style->is_null() ? nullptr : std::make_shared<const Style>(Style::fromJson(*style, reader.getQualifiedName("style")));
    }
    if (reader.has("cursor")) {
        platform::Window::Cursor cursor = platform::Window::Cursor::Default;
        reader.readChoice<platform::Window::Cursor>("cursor", cursor, platform::Window::kCursorNames);
        common.cursor = cursor;
    }
    readFocus(reader);
}

// A node with a theme or a style measures and draws itself and its children with them.
bool Component::pushStyle(Context& context, bool drawing) const {
    if (common.theme.empty() && !common.style) {
        return false;
    }
    context.pushStyle(common.theme, common.style.get(), drawing);
    return true;
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

float Component::clampWidth(float outer) const noexcept {
    const float margin = common.margin.getHorizontal();
    return common.width ? *common.width + margin : getBoundedWidth(outer - margin) + margin;
}

float Component::clampHeight(float outer) const noexcept {
    const float margin = common.margin.getVertical();
    return common.height ? *common.height + margin : getBoundedHeight(outer - margin) + margin;
}

float Component::boundWidth(float outer) const noexcept {
    const float margin = common.margin.getHorizontal();
    return getBoundedWidth(outer - margin) + margin;
}

float Component::boundHeight(float outer) const noexcept {
    const float margin = common.margin.getVertical();
    return getBoundedHeight(outer - margin) + margin;
}

math::Vec2 Component::measure(Context& context, float availableWidth) {
    if (!common.visible || isFloating()) {
        return {};
    }

    // Parents measure a child before drawing it and the child is measured again while it draws, so one answer per frame and width is kept.
    if (measuredFrame == context.getFrame() && measuredWidth == availableWidth) {
        return measuredSize;
    }
    const math::Insets& margin = common.margin;
    const float inner = std::max(0.0F, availableWidth - margin.getHorizontal());
    const float offered = common.width.value_or(getBoundedWidth(inner));
    const bool writing = pushWriting(context);
    const bool styled = pushStyle(context, false);
    const math::Vec2 content = measureContent(context, offered);
    if (styled) {
        context.popStyle();
    }
    if (writing) {
        context.popWriting();
    }
    math::Vec2 size{common.width.value_or(getBoundedWidth(content.x)), common.height.value_or(getBoundedHeight(content.y))};

    // A node with an aspect ratio takes its height from its width, its width from a fixed height, or the whole width it is offered when neither is fixed.
    if (common.aspectRatio) {
        const float ratio = *common.aspectRatio;
        if (common.height && !common.width) {
            size.x = getBoundedWidth(*common.height * ratio);
        } else {
            size.x = common.width.value_or(inner < CommonProperties::kUnbounded ? getBoundedWidth(inner) : size.x);
            size.y = getBoundedHeight(size.x / ratio);
        }
    }
    measuredSize = {size.x + margin.getHorizontal(), size.y + margin.getVertical()};
    measuredFrame = context.getFrame();
    measuredWidth = availableWidth;
    return measuredSize;
}

void Component::draw(Context& context, const math::Rect& layout) {
    if (!common.visible) {
        return;
    }
    math::Rect bounds = layout.inset(common.margin).translated(transform->offset);
    if (common.aspectRatio) {
        bounds = fitAspect(bounds, *common.aspectRatio);
    }
    const bool appeared = drawnFrame == 0 || drawnFrame + 1 != context.getFrame();
    drawnBounds = bounds;
    drawnFrame = context.getFrame();

    if (identity >= 0) {
        ImGui::PushID(identity);
    } else {
        ImGui::PushID(this);
    }
    drawId = ImGui::GetID("##node");
    FocusNavigator& focus = context.getFocus();
    focus.enter(*this, drawId, bounds);
    for (const std::string_view notice : focus.takeNotices(drawId)) {
        context.emit(*this, std::string(notice));
    }
    if (appeared && isListening(Notice::Show)) {
        context.emit(*this, "show");
    }

    // Autofocus takes the focus when the node appears and the focus is elsewhere, such as when its GUI mounts or its dialog opens.
    if (common.autofocus && appeared && isFocusable() && !focus.hasFocusHere()) {
        focusRequested = true;
    }

    const bool styled = pushStyle(context, true);
    if (!common.enabled) {
        ImGui::BeginDisabled();
    }
    const bool writing = pushWriting(context);
    if (common.cursor && isPointerOver(bounds)) {
        context.getBackend().setCursor(*common.cursor);
    }
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
    // A node that is disabled, itself or through a node around it, reports nothing of the pointer.
    if (isListeningToPointer() && (ImGui::GetCurrentContext()->CurrentItemFlags & ImGuiItemFlags_Disabled) == 0) {
        reportPointer(context, bounds);
    }
    if (!common.enabled) {
        ImGui::EndDisabled();
    }
    drawTooltip(context, bounds);
    if (writing) {
        context.popWriting();
    }
    if (styled) {
        context.popStyle();
    }
    focus.leave();
    ImGui::PopID();
}

math::Rect Component::getAnchoredBounds(Context& context) {
    const Backend& backend = context.getBackend();
    const math::Rect area = common.anchorArea == Anchor::Area::Safe ? backend.getSafeRect() : backend.getDisplayRect();
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

// The pointer is over a node inside the visible clip of the window being drawn, while no window above it takes the pointer.
bool Component::isPointerOver(const math::Rect& bounds) {
    return ImGui::IsMouseHoveringRect(ImGuiConverter::toImVec2(bounds.getMin()), ImGuiConverter::toImVec2(bounds.getMax())) && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
}

// Every listening node under the pointer reports it, in the visible part of the node and while no window above takes the pointer.
void Component::reportPointer(Context& context, const math::Rect& bounds) {
    const bool over = isPointerOver(bounds);
    if (over != hovered) {
        hovered = over;
        if (isListening(Notice::Hover)) {
            context.emit(*this, "hover", {{"hovered", over}});
        }
    }
    if (!reportsPresses()) {
        reportPress(context, over);
    }

    const ImGuiIO& io = ImGui::GetIO();
    if (over && isListening(Notice::Scroll) && (io.MouseWheel != 0.0F || io.MouseWheelH != 0.0F)) {
        context.emit(*this, "scroll", {{"deltaX", io.MouseWheelH}, {"deltaY", io.MouseWheel}});
    }
}

// A press that starts over the node follows its button wherever the pointer goes, until the button lets go.
void Component::reportPress(Context& context, bool over) {
    const ImGuiIO& io = ImGui::GetIO();
    const math::Vec2 point = context.toDesign(math::Vec2{io.MousePos.x, io.MousePos.y});
    if (!pressedButton) {
        for (int button = 0; button < static_cast<int>(input::Controls::kMouseButtonCount); ++button) {
            if (!over || !ImGui::IsMouseClicked(button)) {
                continue;
            }
            pressedButton = button;
            if (isListening(Notice::Press)) {
                context.emit(*this, "press", {{"x", point.x}, {"y", point.y}, {"button", getButtonName(button)}});
            }
            break;
        }
        return;
    }

    const int button = *pressedButton;
    if (ImGui::IsMouseDown(button)) {
        if (isListening(Notice::Drag) && (io.MouseDelta.x != 0.0F || io.MouseDelta.y != 0.0F)) {
            context.emit(*this, "drag", {{"x", point.x}, {"y", point.y}, {"deltaX", io.MouseDelta.x}, {"deltaY", io.MouseDelta.y}, {"button", getButtonName(button)}});
        }
        return;
    }
    pressedButton.reset();
    if (isListening(Notice::Release)) {
        context.emit(*this, "release", {{"x", point.x}, {"y", point.y}, {"button", getButtonName(button)}, {"inside", over}});
    }
}

std::string Component::getButtonName(int button) {
    return std::string(input::Controls::mouseButtonName(static_cast<input::MouseButton>(button)));
}

void Component::drawTooltip(Context& context, const math::Rect& bounds) {
    if (common.tooltip.isEmpty() || !isPointerOver(bounds)) {
        hoverStarted = -1.0;
        return;
    }
    if (hoverStarted < 0.0) {
        hoverStarted = context.getTime();
    }
    if (context.getTime() - hoverStarted < context.getMetric(Theme::Metric::TooltipDelay)) {
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
        const math::Rect frame{position.x, position.y, size.x, size.y};
        Surfaces::drawShadow(context, frame, context.getMetric(Theme::Metric::ControlRadius));
        Surfaces::draw(context, Theme::Surface::Tooltip, frame, context.getColor(Theme::Color::Tooltip));
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
        throw std::invalid_argument("The component kind \"" + std::string(getKind()) + "\" does not answer the command \"" + std::string(name) + "\".");
    }
    if (!arguments.is_null() && !(arguments.is_object() && arguments.empty())) {
        throw std::invalid_argument("The \"focus\" command takes no arguments.");
    }
    focusRequested = true;
}

// A node that stops drawing lets go of the pointer, so a hover or a press never stays on while it is hidden.
void Component::noticeStoppedDrawing(Context& context) {
    if (drawnFrame != 0 && drawnFrame + 1 == context.getFrame()) {
        drawingStopped(context);
        if (std::exchange(hovered, false) && isListening(Notice::Hover)) {
            context.emit(*this, "hover", {{"hovered", false}});
        }
        if (const std::optional<int> button = std::exchange(pressedButton, std::nullopt); button && isListening(Notice::Release)) {
            const math::Vec2 point = context.toDesign(math::Vec2{ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y});
            context.emit(*this, "release", {{"x", point.x}, {"y", point.y}, {"button", getButtonName(*button)}, {"inside", false}});
        }
        if (isListening(Notice::Hide)) {
            context.emit(*this, "hide");
        }
    }
    for (const auto& child : children) {
        child->noticeStoppedDrawing(context);
    }
}

bool Component::isPlaced(const std::unique_ptr<Component>& child) noexcept {
    return child->common.visible && !child->common.anchor && !child->isFloating();
}

Component* Component::toPointer(const std::unique_ptr<Component>& child) noexcept {
    return child.get();
}

// A rectangle of another shape keeps the ratio by shrinking along its longer side around its center.
math::Rect Component::fitAspect(const math::Rect& area, float ratio) noexcept {
    if (area.height <= 0.0F || area.width / area.height > ratio) {
        const float width = area.height * ratio;
        return {std::floor(area.x + (area.width - width) * 0.5F), area.y, width, area.height};
    }
    const float height = area.width / ratio;
    return {area.x, std::floor(area.y + (area.height - height) * 0.5F), area.width, height};
}

} // namespace haylen::ui
