#include "ui/components/text/RichText.hpp"

#include <string>
#include <utility>
#include <vector>

#include <imgui.h>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/text/FontFamily.hpp"
#include "haylen/text/Style.hpp"
#include "haylen/ui/Backend.hpp"
#include "haylen/ui/Context.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

// Literal markup is read at once, so malformed markup fails the change that brings it.
void RichText::readProperties(PropertyReader& reader) {
    if (reader.has("text")) {
        reader.read("text", text);
        linked = text.key.empty() && !text::RichText::parse(text.literal).links.empty();
    }
    reader.readChoice<Theme::Font>("font", font, kFonts);
    reader.read("color", color);
    reader.readChoice<text::Alignment>("textAlign", textAlign, text::Style::kAlignmentNames);
    reader.read("wrap", wrap);
    reader.read("revealSpeed", revealSpeed, 0.0F);
    reader.read("visibleCharacters", visibleCharacters, -1);
    changed = true;
}

// The theme gives the family, size, style and color and the node its language and the side start and end name, so a change of them lays the text out again, which starts its reveal over. Paragraphs read in the direction of their first strong letter unless their markup sets one. The text takes the em size of the widgets of its role, so it lines up with labels in the same font. Fonts and images resolve through the context the options were made with.
text::RichText& RichText::prepare(Context& context) {
    std::string markup = context.getText(text);
    std::shared_ptr<text::FontFamily> family = context.getFontFamily(font);
    const float size = context.getEmSize(font);
    const bool bold = context.isFontBold(font);
    const bool italic = context.isFontItalic(font);
    const math::Color ink = context.getColor(color.value_or(Theme::Color::Text));
    const bool rightToLeft = context.isRightToLeft();
    const bool sided = textAlign == text::Alignment::Start || textAlign == text::Alignment::End;
    const text::Alignment align = sided ? ((textAlign == text::Alignment::Start) != rightToLeft ? text::Alignment::Left : text::Alignment::Right) : textAlign;
    const text::RichTextOptions* current = richText ? &richText->getOptions() : nullptr;
    const bool sameOptions = current != nullptr && preparedFor == &context && current->family == family && current->size == size && current->bold == bold && current->italic == italic && current->color == ink && current->align == align && current->language == context.getLanguage() && current->revealSpeed == revealSpeed;

    if (!sameOptions) {
        Context* owner = &context;
        preparedFor = owner;
        text::RichTextOptions options{.family = std::move(family), .size = size, .bold = bold, .italic = italic, .color = ink, .align = align, .language = context.getLanguage(), .revealSpeed = revealSpeed};
        options.fonts = [owner](std::string_view name) { return owner->getFontFamily(name); };
        options.images = [owner](std::string_view path) { return owner->getImage(path); };
        if (richText) {
            richText->setOptions(std::move(options));
        } else {
            richText = std::make_shared<text::RichText>(markup, std::move(options), context.getTextRegistry());
        }
        changed = true;
    }
    if (markup != richText->getMarkup()) {
        richText->setMarkup(std::move(markup));
        hoveredLink.reset();
    }
    linked = !richText->getDocument().links.empty();
    if (std::exchange(changed, false) && visibleCharacters >= 0) {
        richText->setVisibleCharacters(static_cast<std::size_t>(visibleCharacters));
    }
    return *richText;
}

// Text that fits on one line keeps its natural width, and longer text wraps at the width it gets.
math::Vec2 RichText::measureContent(Context& context, float availableWidth) {
    text::RichText& prepared = prepare(context);
    const math::Vec2 natural = prepared.getLayout(0.0F).size;
    if (!wrap || !(availableWidth > 0.0F) || natural.x <= availableWidth) {
        return natural;
    }
    return prepared.getLayout(availableWidth).size;
}

void RichText::render(Context& context, const math::Rect& bounds) {
    text::RichText& prepared = prepare(context);
    if (updatedFrame != context.getFrame()) {
        updatedFrame = context.getFrame();
        prepared.update(context.getDeltaSeconds());
    }
    prepared.setMaxWidth(wrap ? bounds.width : 0.0F);

    // Text that does not wrap aligns as one block inside the bounds, where start and end follow the direction of the UI.
    const float room = bounds.width - prepared.getSize().x;
    const bool rightToLeft = context.isRightToLeft();
    const bool toRight = textAlign == text::Alignment::Right || (textAlign == text::Alignment::Start && rightToLeft) || (textAlign == text::Alignment::End && !rightToLeft);
    const float shift = wrap ? 0.0F : (textAlign == text::Alignment::Center ? room * 0.5F : (toRight ? room : 0.0F));
    const math::Vec2 origin{bounds.x + shift, bounds.y};
    interactWithLinks(context, origin);
    showHint(context, origin);

    // The transforms of the nodes around it scale and color the text the way they reshape the vertices of ImGui.
    const Context::Reshape shape = context.getReshape();
    // clang-format off
    context.getBackend().addRenderCallback([shown = richText, origin, shape](graphics2d::Renderer& renderer, math::Vec2 offset) {
        renderer.drawRichText(*shown, origin * shape.scale + shape.offset + offset, {}, shape.scale, shape.color);
    });
    // clang-format on
}

// Every piece of a wrapped link reacts to the pointer, and only its first piece takes the focus, so the focus stops once per link.
void RichText::interactWithLinks(Context& context, math::Vec2 origin) {
    const text::Layout& layout = richText->getLayout();
    const std::vector<std::string>& links = richText->getDocument().links;
    const bool takesFocus = takeFocusRequest();
    std::vector<bool> reached(links.size(), false);
    std::optional<std::size_t> hovered;

    for (std::size_t index = 0; index < layout.links.size(); ++index) {
        const text::Layout::Area& area = layout.links[index];
        const bool first = !reached[area.index];
        reached[area.index] = true;
        const std::string label = "##link" + std::to_string(index);
        const Widgets::Interaction state = Widgets::interact(context, area.rect.translated(origin), context.getMetric(Theme::Metric::ControlRadius) * 0.5F, label, first ? ImGuiButtonFlags{ImGuiButtonFlags_None} : ImGuiButtonFlags{ImGuiButtonFlags_NoNavFocus});
        if (first && takesFocus && area.index == 0) {
            Widgets::focusItem(context);
        }
        if (state.hovered) {
            hovered = area.index;
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }
        if (state.clicked) {
            context.emit(*this, "link", {{"link", links[area.index]}});
        }
    }

    if (hovered == hoveredLink) {
        return;
    }
    if (hoveredLink) {
        context.emit(*this, "linkHover", {{"link", links[*hoveredLink]}, {"hovered", false}});
    }
    if (hovered) {
        context.emit(*this, "linkHover", {{"link", links[*hovered]}, {"hovered", true}});
    }
    hoveredLink = hovered;
}

void RichText::showHint(Context&, math::Vec2 origin) {
    if (!ImGui::IsWindowHovered()) {
        return;
    }
    for (const text::Layout::Area& area : richText->getLayout().hints) {
        const math::Rect rect = area.rect.translated(origin);
        if (ImGui::IsMouseHoveringRect(ImGuiConverter::toImVec2(rect.getMin()), ImGuiConverter::toImVec2(rect.getMax()))) {
            ImGui::SetTooltip("%s", richText->getDocument().hints[area.index].c_str());
            return;
        }
    }
}

} // namespace haylen::ui
