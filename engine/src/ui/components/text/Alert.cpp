#include "ui/components/text/Alert.hpp"

#include <algorithm>
#include <string>

#include <imgui.h>

#include "haylen/ui/Context.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/Typography.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

void Alert::readProperties(PropertyReader& reader) {
    reader.readChoice<Widgets::Tone>("tone", tone, Widgets::kTones);
    reader.read("title", title);
    reader.read("message", message);
}

math::Vec2 Alert::measureContent(Context& context, float availableWidth) {
    const float padding = context.getMetric(Theme::Metric::ControlPaddingX);
    const float width = std::max(0.0F, availableWidth - padding * 2.0F - kBar);
    math::Vec2 size;
    if (const std::string titleText = context.getText(title); !titleText.empty()) {
        size = Typography::measureParagraph(context, Theme::Font::Button, titleText, width);
    }
    if (const std::string messageText = context.getText(message); !messageText.empty()) {
        const math::Vec2 body = Typography::measureParagraph(context, Theme::Font::Body, messageText, width);
        size = {std::max(size.x, body.x), size.y + body.y};
    }
    return {size.x + padding * 2.0F + kBar, size.y + context.getMetric(Theme::Metric::ControlPaddingY) * 2.0F};
}

void Alert::render(Context& context, const math::Rect& bounds) {
    const Widgets::ToneColors colors = Widgets::getToneColors(tone);
    const float radius = context.getMetric(Theme::Metric::ControlRadius);
    ImDrawList& list = *ImGui::GetWindowDrawList();
    list.AddRectFilled(ImGuiConverter::toImVec2(bounds.getMin()), ImGuiConverter::toImVec2(bounds.getMax()), ImGuiConverter::toImU32(context.getColor(colors.background)), radius);
    list.AddRectFilled(ImGuiConverter::toImVec2(bounds.getMin()), {bounds.x + kBar, bounds.getBottom()}, ImGuiConverter::toImU32(context.getColor(colors.fill)), radius, ImDrawFlags_RoundCornersLeft);

    const float padding = context.getMetric(Theme::Metric::ControlPaddingX);
    const math::Rect inner = math::Rect::fromMinMax({bounds.x + kBar + padding, bounds.y + context.getMetric(Theme::Metric::ControlPaddingY)}, {bounds.getRight() - padding, bounds.getBottom()});
    float y = inner.y;
    if (const std::string titleText = context.getText(title); !titleText.empty()) {
        const float height = Typography::measureParagraph(context, Theme::Font::Button, titleText, inner.width).y;
        Typography::drawParagraph(context, Theme::Font::Button, {inner.x, y, inner.width, height}, context.getColor(colors.text), titleText, Alignment::Start);
        y += height;
    }
    Typography::drawParagraph(context, Theme::Font::Body, {inner.x, y, inner.width, inner.getBottom() - y}, context.getColor(Theme::Color::Text), context.getText(message), Alignment::Start);
}

} // namespace haylen::ui
