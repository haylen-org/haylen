#include "ui/components/overlays/Toast.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <string>

#include <imgui.h>

#include "haylen/ui/Backend.hpp"
#include "haylen/ui/Context.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/Surfaces.hpp"
#include "ui/Typography.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

void Toast::readProperties(PropertyReader& reader) {
    if (reader.has("open")) {
        reader.read("open", open);
        shownAt = -1.0;
    }
    reader.read("text", text);
    reader.readChoice<Widgets::Tone>("tone", tone, Widgets::kTones);
    reader.read("duration", duration, 0.0F, 3600.0F);
    reader.readChoice<Position>("position", position, kPositions);
}

math::Vec2 Toast::measureContent(Context&, float) {
    return {};
}

void Toast::render(Context& context, const math::Rect&) {
    if (!open) {
        return;
    }
    if (shownAt < 0.0) {
        shownAt = context.getTime();
    }
    const auto elapsed = static_cast<float>(context.getTime() - shownAt);
    if (duration > 0.0F && elapsed >= duration) {
        open = false;
        context.emit(*this, "dismiss");
        return;
    }

    constexpr float kFadeIn = 0.2F;
    constexpr float kFadeOut = 0.3F;
    float alpha = std::min(1.0F, elapsed / kFadeIn);
    if (duration > 0.0F) {
        alpha = std::min(alpha, (duration - elapsed) / kFadeOut);
    }

    const math::Rect safe = context.getBackend().getSafeRect();
    const float margin = context.getMetric(Theme::Metric::PanelPadding);
    const float padding = context.getMetric(Theme::Metric::ControlPaddingX);
    const float width = std::min(context.getMetric(Theme::Metric::ToastWidth), safe.width - margin * 2.0F);
    const std::string message = context.getText(text);
    const float height = Typography::measureParagraph(context, Theme::Font::Body, message, width - padding * 2.0F - kBar).y + context.getMetric(Theme::Metric::ControlPaddingY) * 2.0F;
    const float y = position == Position::Top ? safe.y + margin : safe.getBottom() - margin - height;
    const math::Rect frame{std::floor(safe.getCenter().x - width * 0.5F), std::floor(y), width, height};
    setBounds(frame);

    ImGui::SetNextWindowPos(ImGuiConverter::toImVec2(frame.getMin()));
    ImGui::SetNextWindowSize(ImGuiConverter::toImVec2(frame.getSize()));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, std::clamp(alpha, 0.0F, 1.0F));
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin(std::format("##toast{}", static_cast<const void*>(this)).c_str(), nullptr, flags)) {
        const Widgets::ToneColors colors = Widgets::getToneColors(tone);
        Surfaces::draw(context, Theme::Surface::Toast, frame, context.getColor(Theme::Color::Tooltip), std::nullopt);
        const math::Rect bar = context.mirror({frame.x, frame.y, kBar, frame.height}, frame);
        ImGui::GetWindowDrawList()->AddRectFilled(ImGuiConverter::toImVec2(bar.getMin()), ImGuiConverter::toImVec2(bar.getMax()), ImGuiConverter::toImU32(context.getColor(colors.fill)), context.getMetric(Theme::Metric::ControlRadius), context.isRightToLeft() ? ImDrawFlags_RoundCornersRight : ImDrawFlags_RoundCornersLeft);
        const math::Rect inner = context.mirror(math::Rect::fromMinMax({frame.x + kBar + padding, frame.y + context.getMetric(Theme::Metric::ControlPaddingY)}, {frame.getRight() - padding, frame.getBottom()}), frame);
        Typography::drawParagraph(context, Theme::Font::Body, inner, context.getColor(Theme::Color::OnTooltip), message, Alignment::Start);
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace haylen::ui
