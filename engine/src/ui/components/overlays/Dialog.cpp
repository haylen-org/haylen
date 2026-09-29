#include "ui/components/overlays/Dialog.hpp"

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>

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
    const bool shown = ImGui::IsPopupOpen("##dialog");
    if (open && !shown) {
        ImGui::OpenPopup("##dialog");
    }

    const math::Rect display = context.getBackend().getDisplayRect();
    const math::Insets padding = getContentPadding(context);
    const float width = std::min(context.getMetric(Theme::Metric::DialogWidth), display.width - context.getMetric(Theme::Metric::PanelPadding) * 2.0F);
    const float inner = width - padding.getHorizontal();
    const float height = getContentHeight(context, inner) + padding.getVertical();
    ImGui::SetNextWindowPos(ImGuiConverter::toImVec2(display.getCenter()), ImGuiCond_Always, {0.5F, 0.5F});
    ImGui::SetNextWindowSize({width, height});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0F, 0.0F});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.0F, 0.0F, 0.0F, 0.0F));
    const bool began = ImGui::BeginPopupModal("##dialog", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNavInputs);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    if (!began) {
        return;
    }

    const math::Rect frame{ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, width, height};
    setBounds(frame);
    Surfaces::draw(context, Theme::Surface::Dialog, frame, context.getColor(Theme::Color::Panel), context.getColor(Theme::Color::Border));
    drawContent(context, frame.inset(padding));

    if (open && dismissible && context.getFocus().answerCancel(ImGui::GetCurrentWindow())) {
        open = false;
        context.emit(*this, "dismiss");
    }
    if (!open) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
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
        core::JsonValidator::requireKnownKeys(entry, {"id", "text", "variant"}, "dialog.buttons");
        Answer button{.id = entry.at("id").get<std::string>()};
        if (!ids.insert(button.id).second) {
            reader.fail("buttons", "uses the id " + button.id + " more than once");
        }
        if (const auto text = entry.find("text"); text != entry.end()) {
            button.text = TextValue::fromJson(*text, "dialog.buttons.text");
        }
        if (const auto variant = entry.find("variant"); variant != entry.end()) {
            const auto found = std::ranges::find_if(Widgets::kButtonVariants, [&](const auto& choice) { return *variant == choice.first; });
            if (found == Widgets::kButtonVariants.end()) {
                reader.fail("buttons", "has an unknown variant");
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

float Dialog::getContentHeight(Context& context, float width) {
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
    if (!buttons.empty()) {
        add(context.getMetric(Theme::Metric::ControlHeight));
    }
    return height;
}

void Dialog::drawContent(Context& context, const math::Rect& inner) {
    const float spacing = context.getMetric(Theme::Metric::ItemSpacing);
    float y = inner.y;
    if (const std::string titleText = context.getText(title); !titleText.empty()) {
        const float height = Typography::measureParagraph(context, Theme::Font::Heading, titleText, inner.width).y;
        Typography::drawParagraph(context, Theme::Font::Heading, {inner.x, y, inner.width, height}, context.getColor(Theme::Color::Text), titleText, Alignment::Start);
        y += height + spacing;
    }
    if (const std::string messageText = context.getText(message); !messageText.empty()) {
        const float height = Typography::measureParagraph(context, Theme::Font::Body, messageText, inner.width).y;
        Typography::drawParagraph(context, Theme::Font::Body, {inner.x, y, inner.width, height}, context.getColor(Theme::Color::TextMuted), messageText, Alignment::Start);
        y += height + spacing;
    }
    for (Component* child : getLayoutChildren()) {
        const math::Vec2 size = child->measure(context, inner.width);
        child->draw(context, {inner.x, y, inner.width, size.y});
        y += size.y + spacing;
    }

    // Buttons line up at the bottom end, the right or the left of a right-to-left UI, and the last one, usually the main answer, starts with the navigation focus.
    float x = inner.getRight();
    const float height = context.getMetric(Theme::Metric::ControlHeight);
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

} // namespace haylen::ui
