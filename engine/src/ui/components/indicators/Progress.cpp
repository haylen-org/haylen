#include "ui/components/indicators/Progress.hpp"

#include <algorithm>
#include <string>
#include <utility>

#include <imgui.h>

#include "haylen/ui/AccessibilityTree.hpp"
#include "haylen/ui/Context.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/Typography.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

void Progress::readProperties(PropertyReader& reader) {
    reader.read("value", value, 0.0F, 1.0F);
    reader.readChoice<Widgets::Tone>("tone", tone, Widgets::kTones);
    reader.read("text", text);
}

math::Vec2 Progress::measureContent(Context& context, float availableWidth) {
    return {std::min(availableWidth, 320.0F), std::max(context.getMetric(Theme::Metric::ProgressHeight), text.isEmpty() ? 0.0F : Typography::getLineHeight(context, Theme::Font::Caption) + 8.0F)};
}

// The label draws twice, cut where the fill ends across the bar: in the ink of the tone over the fill and in the text color over the empty track, so every letter keeps its contrast. The fill grows from the side the UI starts.
void Progress::render(Context& context, const math::Rect& bounds) {
    const math::Rect filled = Widgets::progress(context, bounds, value, tone);
    const std::string_view shown = context.getText(text);
    if (shown.empty()) {
        return;
    }
    const float lineHeight = Typography::getLineHeight(context, Theme::Font::Caption);
    const math::Rect line{bounds.x, bounds.getCenter().y - lineHeight * 0.5F, bounds.width, lineHeight};
    const float edge = filled.isEmpty() ? (context.isRightToLeft() ? bounds.getRight() : bounds.x) : (context.isRightToLeft() ? filled.x : filled.getRight());
    const math::Rect start = math::Rect::fromMinMax(bounds.getMin(), {edge, bounds.getBottom()});
    const math::Rect end = math::Rect::fromMinMax({edge, bounds.y}, bounds.getMax());
    const math::Color ink = context.getColor(Widgets::getToneColors(tone == Widgets::Tone::Neutral ? Widgets::Tone::Accent : tone).ink);
    const math::Color plain = context.getColor(Theme::Color::Text);
    for (const auto& [part, color] : {std::pair{start, context.isRightToLeft() ? plain : ink}, std::pair{end, context.isRightToLeft() ? ink : plain}}) {
        if (part.width <= 0.0F) {
            continue;
        }
        const math::Rect clip = context.getReshape().apply(part);
        ImGui::PushClipRect(ImGuiConverter::toImVec2(clip.getMin()), ImGuiConverter::toImVec2(clip.getMax()), true);
        Typography::drawParagraph(context, Theme::Font::Caption, line, color, shown, Alignment::Center);
        ImGui::PopClipRect();
    }
}

bool Progress::describe(Context& context, platform::AccessibilityElement& element) const {
    element.role = platform::AccessibilityElement::Role::ProgressBar;
    element.label.assign(context.getText(text));
    AccessibilityTree::appendNumber(element.value, static_cast<double>(value) * 100.0, 0);
    element.value.push_back('%');
    return true;
}

} // namespace haylen::ui
