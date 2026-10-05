#include "ui/components/overlays/Dialog.hpp"

#include <algorithm>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include "haylen/core/JsonValidator.hpp"
#include "haylen/math/Insets.hpp"
#include "haylen/ui/Backend.hpp"
#include "haylen/ui/Context.hpp"
#include "haylen/ui/FocusNavigator.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/Surfaces.hpp"
#include "ui/Typography.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

void Dialog::readProperties(PropertyReader& reader) {
    reader.read("open", open);
    reader.read("title", title);
    reader.read("message", message);
    reader.read("dismissible", dismissible);
    if (const core::Json* parsed = reader.take("buttons")) {
        buttons = readButtons(reader, *parsed);
    }
}

math::Vec2 Dialog::measureContent(Context&, float) {
    return {};
}

void Dialog::render(Context& context, const math::Rect&) {
    if (open && !ImGui::IsPopupOpen("##dialog") && !isWaiting()) {
        ImGui::OpenPopup("##dialog");
    }
    if (!ImGui::IsPopupOpen("##dialog")) {
        shown = 0.0F;
        return;
    }

    // The dialog and its backdrop fade in and out together over the transition duration of the theme, and a closing dialog stays until it faded, taking no answers.
    const float duration = context.getMetric(Theme::Metric::TransitionDuration);
    const float step = duration > 0.0F ? context.getDeltaSeconds() / duration : 1.0F;
    shown = open ? std::min(1.0F, shown + step) : std::max(0.0F, shown - step);
    if (shown <= 0.0F) {
        close();
        return;
    }

    // A dialog keeps a margin inside the display, and its content scrolls when it is taller than that.
    const math::Rect display = context.getBackend().getDisplayRect();
    const float margin = context.getMetric(Theme::Metric::PanelPadding);
    const math::Insets padding = getContentPadding(context);
    const float width = std::min(context.getMetric(Theme::Metric::DialogWidth), display.width - margin * 2.0F);
    const float inner = width - padding.getHorizontal();
    const float height = std::min(getContentHeight(context, inner) + padding.getVertical(), std::max(0.0F, display.height - margin * 2.0F));
    ImGui::SetNextWindowPos(ImGuiConverter::toImVec2(display.getCenter()), ImGuiCond_Always, {0.5F, 0.5F});
    ImGui::SetNextWindowSize({width, height});
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * shown);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0F, 0.0F});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.0F, 0.0F, 0.0F, 0.0F));
    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.0F, 0.0F, 0.0F, 0.0F));
    const bool began = ImGui::BeginPopupModal("##dialog", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNavInputs);
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
    if (!began) {
        ImGui::PopStyleVar();
        return;
    }

    const math::Rect frame{ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, width, height};
    setBounds(frame);
    drawBackdrop(context, display);
    const float radius = context.getMetric(Theme::Metric::PanelRadius);
    Surfaces::drawShadow(context, frame, radius);
    Surfaces::draw(context, Theme::Surface::Dialog, frame, context.getColor(Theme::Color::Panel), context.getColor(Theme::Color::Border), radius);
    drawContent(context, frame, padding);

    if (open && dismissible && context.getFocus().answerCancel(ImGui::GetCurrentWindow())) {
        open = false;
        context.emit(*this, "dismiss");
    }
    ImGui::EndPopup();
    ImGui::PopStyleVar();
}

// A faded dialog closes its popup from outside, since ImGui skips the window of a popup drawn with no opacity.
void Dialog::close() {
    const ImGuiContext& state = *GImGui;
    const ImGuiID id = ImGui::GetID("##dialog");
    for (int level = 0; level < state.OpenPopupStack.Size; ++level) {
        if (state.OpenPopupStack[level].PopupId == id) {
            ImGui::ClosePopupToLevel(level, true);
            return;
        }
    }
}

// The backdrop covers the whole display behind the dialog, drawn first in the window of the dialog so it stays above the GUIs under it.
void Dialog::drawBackdrop(Context& context, const math::Rect& display) {
    ImDrawList& list = *ImGui::GetWindowDrawList();
    list.PushClipRect(ImGuiConverter::toImVec2(display.getMin()), ImGuiConverter::toImVec2(display.getMax()), false);
    list.AddRectFilled(ImGuiConverter::toImVec2(display.getMin()), ImGuiConverter::toImVec2(display.getMax()), ImGuiConverter::toImU32(context.getColor(Theme::Color::Overlay)));
    list.PopClipRect();
}

// Only one dialog shows at a time, so a dialog that opens while another one shows waits until that one closes.
bool Dialog::isWaiting() {
    const ImGuiContext& state = *GImGui;
    const int level = state.BeginPopupStack.Size;
    if (state.OpenPopupStack.Size <= level) {
        return false;
    }
    const ImGuiWindow* shown = state.OpenPopupStack[level].Window;
    return shown != nullptr && (shown->Flags & ImGuiWindowFlags_Modal) != 0 && (shown->Active || shown->WasActive);
}

std::vector<Dialog::Answer> Dialog::readButtons(PropertyReader& reader, const core::Json& value) {
    if (!PropertyReader::isList(value)) {
        reader.fail("buttons", "must be a list");
    }
    std::vector<Answer> parsed;
    std::set<std::string, std::less<>> ids;
    for (const core::Json& entry : value) {
        if (!entry.is_object() || !entry.contains("id") || !entry.at("id").is_string()) {
            reader.fail("buttons", "must hold objects with an id");
        }
        core::JsonValidator::requireKnownKeys(entry, {"id", "text", "variant"}, "\"dialog.buttons\"");
        Answer button{.id = entry.at("id").get<std::string>()};
        if (!ids.insert(button.id).second) {
            reader.fail("buttons", "uses the id \"" + button.id + "\" more than once");
        }
        if (const auto text = entry.find("text"); text != entry.end()) {
            button.text = TextValue::fromJson(*text, "dialog.buttons.text");
        }
        if (const auto variant = entry.find("variant"); variant != entry.end()) {
            const auto found = std::ranges::find_if(Widgets::kButtonVariants, [&](const auto& choice) { return variant->is_string() && variant->get_ref<const std::string&>() == choice.first; });
            if (found == Widgets::kButtonVariants.end()) {
                reader.fail("buttons", "has a variant other than \"default\", \"primary\", \"destructive\", \"toolbar\", \"icon\" or \"link\"");
            }
            button.variant = found->second;
        }
        parsed.push_back(std::move(button));
    }
    return parsed;
}

math::Insets Dialog::getContentPadding(Context& context) {
    const math::Insets image = Surfaces::getPadding(context, Theme::Surface::Dialog);
    const float panel = context.getMetric(Theme::Metric::PanelPadding);
    return {image.left + panel, image.top + panel, image.right + panel, image.bottom + panel};
}

float Dialog::getBodyHeight(Context& context, float width) {
    const float spacing = context.getMetric(Theme::Metric::ItemSpacing);
    float height = 0.0F;
    const auto add = [&](float part) { height += part + (height > 0.0F ? spacing : 0.0F); };
    if (const std::string titleText = context.getText(title); !titleText.empty()) {
        add(Typography::measureParagraph(context, Theme::Font::Heading, titleText, width).y);
    }
    if (const std::string messageText = context.getText(message); !messageText.empty()) {
        add(Typography::measureParagraph(context, Theme::Font::Body, messageText, width).y);
    }
    for (Component* child : getLayoutChildren()) {
        add(child->measure(context, width).y);
    }
    return height;
}

float Dialog::getContentHeight(Context& context, float width) {
    const float body = getBodyHeight(context, width);
    if (buttons.empty()) {
        return body;
    }
    return body + (body > 0.0F ? context.getMetric(Theme::Metric::ItemSpacing) : 0.0F) + context.getMetric(Theme::Metric::ControlHeight);
}

void Dialog::drawContent(Context& context, const math::Rect& frame, const math::Insets& padding) {
    const float spacing = context.getMetric(Theme::Metric::ItemSpacing);
    const float height = context.getMetric(Theme::Metric::ControlHeight);
    const math::Rect inner = frame.inset(padding);

    // The title, the message and the children scroll in the room the buttons leave, across the whole width inside the frame, so the scroll bar keeps to the edge of the dialog while the content keeps its padding.
    const math::Insets edge = Surfaces::getFrame(context, Theme::Surface::Dialog);
    const math::Rect body = math::Rect::fromMinMax({frame.x + edge.left, inner.y}, {frame.getRight() - edge.right, std::max(inner.y, inner.getBottom() - (buttons.empty() ? 0.0F : height + spacing))});
    if (!body.isEmpty()) {
        ImGui::SetCursorScreenPos(ImGuiConverter::toImVec2(body.getMin()));
        if (ImGui::BeginChild("##body", ImGuiConverter::toImVec2(body.getSize()), ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoScrollbar)) {
            drawBody(context, body, inner);
        }
        ImGui::EndChild();
    }

    // Buttons line up at the bottom end, the right or the left of a right-to-left UI, and the last one, usually the main answer, starts with the navigation focus.
    float x = inner.getRight();
    for (auto button = buttons.rbegin(); button != buttons.rend(); ++button) {
        const std::string text = context.getText(button->text);
        const math::Vec2 size = Widgets::measureButton(context, text, false, button->variant);
        x -= size.x;
        ImGui::PushID(button->id.c_str());
        const bool pressed = Widgets::button(context, context.mirror({x, inner.getBottom() - height, size.x, height}, inner), text, nullptr, button->variant);
        if (button == buttons.rbegin() && ImGui::IsWindowAppearing()) {
            Widgets::focusItem(context);
        }
        ImGui::PopID();
        x -= spacing;
        if (pressed && open) {
            open = false;
            context.emit(*this, "answer", {{"button", button->id}});
        }
    }
}

// The content keeps the padding of the dialog, and while it is taller than the body it gives up the part of the lane of the scroll bar that its padding leaves, and draws only outside the lane.
void Dialog::drawBody(Context& context, const math::Rect& body, const math::Rect& inner) {
    const float spacing = context.getMetric(Theme::Metric::ItemSpacing);
    const math::Rect padded{inner.x, body.y, inner.width, body.height};
    const bool overflowing = getBodyHeight(context, inner.width) > body.height;
    const math::Rect column = overflowing ? Scrollbar::getContentBox(context, body, padded, false) : padded;
    if (overflowing) {
        const double maximum = std::max(0.0F, getBodyHeight(context, column.width) - body.height);
        if (const std::optional<double> offset = scrollbar.interact(context, body, false, false, ImGui::GetScrollY(), maximum, body.height)) {
            ImGui::SetScrollY(static_cast<float>(*offset));
        }
    }

    const math::Rect clip = overflowing ? Scrollbar::getContentBox(context, body, body, false) : body;
    ImGui::PushClipRect(ImGuiConverter::toImVec2(clip.getMin()), ImGuiConverter::toImVec2(clip.getMax()), true);
    const ImVec2 start = ImGui::GetCursorScreenPos();
    float y = start.y;
    if (const std::string titleText = context.getText(title); !titleText.empty()) {
        const float height = Typography::measureParagraph(context, Theme::Font::Heading, titleText, column.width).y;
        Typography::drawParagraph(context, Theme::Font::Heading, {column.x, y, column.width, height}, context.getColor(Theme::Color::Text), titleText, Alignment::Start);
        y += height + spacing;
    }
    if (const std::string messageText = context.getText(message); !messageText.empty()) {
        const float height = Typography::measureParagraph(context, Theme::Font::Body, messageText, column.width).y;
        Typography::drawParagraph(context, Theme::Font::Body, {column.x, y, column.width, height}, context.getColor(Theme::Color::TextMuted), messageText, Alignment::Start);
        y += height + spacing;
    }
    for (Component* child : getLayoutChildren()) {
        const math::Vec2 size = child->measure(context, column.width);
        child->draw(context, {column.x, y, column.width, size.y});
        y += size.y + spacing;
    }
    ImGui::PopClipRect();
    ImGui::SetCursorScreenPos(start);
    ImGui::Dummy({body.width, std::max(0.0F, y - spacing - start.y)});
    if (overflowing) {
        scrollbar.draw(context, 1.0F);
    }
}

} // namespace haylen::ui
