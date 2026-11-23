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
#include "haylen/ui/AccessibilityTree.hpp"
#include "haylen/ui/Backend.hpp"
#include "haylen/ui/Context.hpp"
#include "haylen/ui/FocusNavigator.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/ScrollFade.hpp"
#include "ui/Surfaces.hpp"
#include "ui/Typography.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

void Dialog::readProperties(PropertyReader& reader) {
    reader.read("title", title);
    reader.read("message", message);
    reader.read("glass", glass);
    if (const core::Json* parsed = reader.take("buttons")) {
        buttons = readButtons(reader, *parsed);
    }
}

// The card keeps a margin inside the room it has and inside the safe area, and its content scrolls only when it is taller than that, which the sizes of the card decide rather than its edges, whose fractions round differently.
math::Vec2 Dialog::measureContent(Context& context, float availableWidth) {
    const float margin = context.getMetric(Theme::Metric::PanelPadding);
    const math::Insets padding = getContentPadding(context);
    const float width = std::max(0.0F, std::min(context.getMetric(Theme::Metric::DialogWidth), availableWidth - margin * 2.0F));
    const float content = getContentHeight(context, width - padding.getHorizontal()) + padding.getVertical();
    const float room = std::max(0.0F, context.getBackend().getLiftedSafeRect().height - margin * 2.0F);
    return {width, std::min(content, room)};
}

void Dialog::render(Context& context, const math::Rect& bounds) {
    const math::Insets padding = getContentPadding(context);
    const float content = getContentHeight(context, bounds.width - padding.getHorizontal()) + padding.getVertical();
    const bool overflowing = content > bounds.height + 0.5F;

    // Glass shows what lies behind it, which a shadow under it would darken.
    const float radius = context.getMetric(Theme::Metric::PanelRadius);
    if (glass) {
        Surfaces::drawGlass(context, bounds, context.getColor(Theme::Color::Border), radius);
    } else {
        Surfaces::drawShadow(context, bounds, radius);
        Surfaces::draw(context, Theme::Surface::Dialog, bounds, context.getColor(Theme::Color::Panel), context.getColor(Theme::Color::Border), radius);
    }

    drawContent(context, bounds, padding, overflowing);
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
    if (const std::string_view titleText = context.getText(title); !titleText.empty()) {
        const math::Insets bar = Surfaces::getPadding(context, Theme::Surface::TitleBar);
        add(Typography::measureParagraph(context, Theme::Font::Heading, titleText, width - bar.getHorizontal()).y + bar.getVertical());
    }

    if (const std::string_view messageText = context.getText(message); !messageText.empty()) {
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

    return body + (body > 0.0F ? context.getMetric(Theme::Metric::ItemSpacing) : 0.0F) + getButtonsHeight(context, width);
}

// The answers line up in one row while they fit the width, and otherwise stack across it.
bool Dialog::isStacked(Context& context, float width) const {
    return getRowWidth(context) > width;
}

float Dialog::getRowWidth(Context& context) const {
    const float spacing = context.getMetric(Theme::Metric::ItemSpacing);
    float row = 0.0F;
    for (const Answer& button : buttons) {
        row += Widgets::measureButton(context, context.getText(button.text), false, button.variant, {}).x + (row > 0.0F ? spacing : 0.0F);
    }

    return row;
}

// A stacked answer takes at least the height of a control, and grows when its label wraps across the width.
float Dialog::getStackedButtonHeight(Context& context, const Answer& button, float width) {
    const float measured = Widgets::measureButton(context, context.getText(button.text), false, button.variant, {}, width).y;
    return std::max(measured, context.getMetric(Theme::Metric::ControlHeight));
}

float Dialog::getButtonsHeight(Context& context, float width) const {
    if (!isStacked(context, width)) {
        return context.getMetric(Theme::Metric::ControlHeight);
    }

    const float spacing = context.getMetric(Theme::Metric::ItemSpacing);
    float height = 0.0F;
    for (const Answer& button : buttons) {
        height += getStackedButtonHeight(context, button, width) + (height > 0.0F ? spacing : 0.0F);
    }

    return height;
}

void Dialog::drawContent(Context& context, const math::Rect& frame, const math::Insets& padding, bool overflowing) {
    const float spacing = context.getMetric(Theme::Metric::ItemSpacing);
    const float controlHeight = context.getMetric(Theme::Metric::ControlHeight);
    const math::Rect inner = frame.inset(padding);
    const float buttonsHeight = buttons.empty() ? 0.0F : getButtonsHeight(context, inner.width);

    // The title, the message and the children scroll in the room the buttons leave, across the whole width inside the frame, so the content scrolls under the padding of the dialog to its edge.
    const math::Insets edge = Surfaces::getFrame(context, Theme::Surface::Dialog);
    const math::Rect body = math::Rect::fromMinMax({frame.x + edge.left, inner.y}, {frame.getRight() - edge.right, std::max(inner.y, inner.getBottom() - (buttons.empty() ? 0.0F : buttonsHeight + spacing))});
    if (!body.isEmpty()) {
        ImGui::SetCursorScreenPos(ImGuiConverter::toImVec2(body.getMin()));

        if (context.getBackend().beginChild("##body", body.getSize(), ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
            drawBody(context, body, inner, overflowing);
        }

        ImGui::EndChild();
    }

    // Buttons line up at the bottom end, the right or the left of a right-to-left UI, or stack across the width when they do not fit in one row, with the last one, usually the main answer, at the end of the row or on top of the stack. That one starts with the navigation focus. They draw in the order people read them, from the start of the row or the top of the stack, so the focus and screen readers move through them in that order.
    const float row = getRowWidth(context);
    const bool stacked = row > inner.width;
    const std::size_t count = buttons.size();
    float x = inner.getRight() - row;
    float y = inner.getBottom() - buttonsHeight;
    for (std::size_t order = 0; order < count; ++order) {
        const std::size_t index = stacked ? count - 1 - order : order;
        const Answer& button = buttons[index];
        const std::string_view text = context.getText(button.text);
        math::Rect place;
        if (stacked) {
            const float height = getStackedButtonHeight(context, button, inner.width);
            place = {inner.x, y, inner.width, height};
            y += height + spacing;
        } else {
            const float width = Widgets::measureButton(context, text, false, button.variant, {}).x;
            place = context.mirror({x, inner.getBottom() - controlHeight, width, controlHeight}, inner);
            x += width + spacing;
        }

        ImGui::PushID(button.id.c_str());
        const bool pressed = Widgets::button(context, place, text, nullptr, button.variant, false, {}, {});
        context.getAccessibility().addPart(context, ImGui::GetItemID(), place, platform::AccessibilityElement::Role::Button, text, 0);

        if (index == count - 1 && !focused) {
            focused = true;
            Widgets::focusItem(context);
        }

        ImGui::PopID();

        if (pressed) {
            context.playSound(Theme::Sound::Press);
            context.emit(*this, "answer", {{"button", button.id}});
        }
    }
}

// The content keeps the padding of the dialog and fades out along the edges of the body where it continues.
void Dialog::drawBody(Context& context, const math::Rect& body, const math::Rect& inner, bool overflowing) {
    const float spacing = context.getMetric(Theme::Metric::ItemSpacing);
    const math::Rect column{inner.x, body.y, inner.width, body.height};
    ImGui::PushClipRect(ImGuiConverter::toImVec2(body.getMin()), ImGuiConverter::toImVec2(body.getMax()), true);

    // The part of a body that scrolls that the on-screen keyboard covers joins the end of the content, so what lies under the keyboard scrolls above it, and no fade shows along that empty room.
    const float cover = overflowing ? context.getBackend().getKeyboardCover(body) : 0.0F;
    const double shown = ImGui::GetScrollY();
    const double rest = std::max(0.0, ImGui::GetScrollMaxY() - static_cast<double>(cover) - shown);
    const bool faded = overflowing && ScrollFade::begin(context, body, false, shown, rest);
    const ImVec2 start = ImGui::GetCursorScreenPos();
    float y = start.y;
    if (const std::string_view titleText = context.getText(title); !titleText.empty()) {
        // An image of the title bar frames the title across the column, and its padding keeps the title inside its border.
        const math::Insets bar = Surfaces::getPadding(context, Theme::Surface::TitleBar);
        const float height = Typography::measureParagraph(context, Theme::Font::Heading, titleText, column.width - bar.getHorizontal()).y + bar.getVertical();
        const math::Rect band{column.x, y, column.width, height};
        if (const Theme::Image* image = context.getSurface(Theme::Surface::TitleBar)) {
            Surfaces::drawNineSlice(context, *image, band, context.getColor(Theme::Color::Raised));
        }

        Typography::drawParagraph(context, Theme::Font::Heading, band.inset(bar), context.getColor(Theme::Color::Text), titleText, Alignment::Start);
        y += height + spacing;
    }

    if (const std::string_view messageText = context.getText(message); !messageText.empty()) {
        const float height = Typography::measureParagraph(context, Theme::Font::Body, messageText, column.width).y;
        Typography::drawParagraph(context, Theme::Font::Body, {column.x, y, column.width, height}, context.getColor(Theme::Color::TextMuted), messageText, Alignment::Start);
        y += height + spacing;
    }

    for (Component* child : getLayoutChildren()) {
        const math::Vec2 size = child->measure(context, column.width);
        child->draw(context, {column.x, y, column.width, size.y});
        y += size.y + spacing;
    }

    if (faded) {
        ScrollFade::end(context);
    }

    ImGui::PopClipRect();
    ImGui::SetCursorScreenPos(start);
    ImGui::Dummy({body.width, std::max(0.0F, y - spacing - start.y) + cover});

    if (overflowing) {
        scroll.updateWindow(context);
    }
}

bool Dialog::describe(Context& context, platform::AccessibilityElement& element) const {
    element.role = platform::AccessibilityElement::Role::Dialog;
    element.label.assign(context.getText(title));
    element.value.assign(context.getText(message));
    return true;
}

} // namespace haylen::ui
