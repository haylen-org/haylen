#include "ui/components/indicators/StatusIndicator.hpp"

#include <algorithm>

#include <imgui.h>

#include "haylen/ui/Context.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/Typography.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

void StatusIndicator::readProperties(PropertyReader& reader) {
    reader.read("text", text);
    reader.readChoice<Widgets::Tone>("tone", tone, Widgets::kTones);
}

math::Vec2 StatusIndicator::measureContent(Context& context, float) {
    const math::Vec2 textSize = Typography::measure(context, Theme::Font::Caption, context.getText(text));
    const float dot = getDotSize(context);
    return {dot + (textSize.x > 0.0F ? dot + textSize.x : 0.0F), std::max(dot, textSize.y)};
}

void StatusIndicator::render(Context& context, const math::Rect& bounds) {
    const float dot = getDotSize(context);
    ImGui::GetWindowDrawList()->AddCircleFilled({bounds.x + dot * 0.5F, bounds.getCenter().y}, dot * 0.5F, ImGuiConverter::toImU32(context.getColor(Widgets::getToneColors(tone).fill)));
    Typography::drawAligned(context, Theme::Font::Caption, {bounds.x + dot * 2.0F, bounds.y, bounds.width - dot * 2.0F, bounds.height}, context.getColor(Theme::Color::TextMuted), context.getText(text), Alignment::Start);
}

float StatusIndicator::getDotSize(Context& context) {
    return context.getFontSize(Theme::Font::Caption) * 0.5F;
}

} // namespace haylen::ui
