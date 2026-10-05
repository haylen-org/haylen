#include "ui/components/indicators/CircularProgress.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <imgui.h>

#include "haylen/ui/Context.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/Surfaces.hpp"
#include "ui/Typography.hpp"

namespace haylen::ui {

void CircularProgress::readProperties(PropertyReader& reader) {
    reader.read("value", value, 0.0F, 1.0F);
    reader.read("size", size, 0.0F, 2048.0F);
    reader.read("thickness", thickness, 0.0F, 512.0F);
    reader.readChoice<Variant>("variant", variant, kVariants);
    reader.readChoice<Widgets::Tone>("tone", tone, Widgets::kTones);
    reader.read("text", text);
    reader.read("image", image);
}

math::Vec2 CircularProgress::measureContent(Context& context, float) {
    const float side = size > 0.0F ? size : context.getMetric(Theme::Metric::CircularProgressSize);
    return {side, side};
}

void CircularProgress::render(Context& context, const math::Rect& bounds) {
    const float radius = std::min(bounds.width, bounds.height) * 0.5F;
    const math::Vec2 center = bounds.getCenter();
    if (variant == Variant::Ring) {
        drawRing(context, center, radius);
    } else {
        drawCooldown(context, bounds, center, radius);
    }

    const std::string shown = context.getText(text);
    if (shown.empty()) {
        return;
    }
    const Theme::Font font = radius < 40.0F ? Theme::Font::Caption : Theme::Font::Body;
    const float line = Typography::getLineHeight(context, font);
    const math::Rect label{center.x - radius, center.y - line * 0.5F, radius * 2.0F, line};
    if (variant == Variant::Ring) {
        Typography::drawAligned(context, font, label, context.getColor(Theme::Color::Text), shown, Alignment::Center);
    } else {
        Typography::drawParagraph(context, font, label, context.getColor(Theme::Color::Text), shown, Alignment::Center, context.getColor(Theme::Color::Window), 2.0F);
    }
}

void CircularProgress::drawRing(Context& context, math::Vec2 center, float radius) const {
    const float width = thickness > 0.0F ? thickness : context.getMetric(Theme::Metric::CircularProgressThickness);
    const float middle = std::max(0.0F, radius - width * 0.5F);
    ImDrawList& list = *ImGui::GetWindowDrawList();
    list.AddCircle(ImGuiConverter::toImVec2(center), middle, ImGuiConverter::toImU32(context.getColor(Theme::Color::Border)), kSegments, width);
    if (value <= 0.0F) {
        return;
    }

    // The fill starts at the top and runs clockwise, which is the direction screen angles grow in.
    constexpr float kTop = -std::numbers::pi_v<float> * 0.5F;
    const Widgets::Tone fillTone = tone == Widgets::Tone::Neutral ? Widgets::Tone::Accent : tone;
    list.PathClear();
    list.PathArcTo(ImGuiConverter::toImVec2(center), middle, kTop, kTop + std::numbers::pi_v<float> * 2.0F * value, kSegments);
    list.PathStroke(ImGuiConverter::toImU32(context.getColor(Widgets::getToneColors(fillTone).fill)), width);
}

void CircularProgress::drawCooldown(Context& context, const math::Rect& bounds, math::Vec2 center, float radius) const {
    const math::Rect square{center.x - radius, center.y - radius, radius * 2.0F, radius * 2.0F};
    if (!image.empty()) {
        Surfaces::drawImage(context, context.getImage(image, square.getSize()), square);
    } else {
        Surfaces::draw(context, Theme::Surface::Badge, bounds, context.getColor(Widgets::getToneColors(tone).background), std::nullopt, radius);
    }
    if (value <= 0.0F) {
        return;
    }

    // The shade covers the part of the circle still waiting and uncovers the picture clockwise from the top as the value falls.
    constexpr float kTwoPi = std::numbers::pi_v<float> * 2.0F;
    const float start = -std::numbers::pi_v<float> * 0.5F + kTwoPi * (1.0F - value);
    const int steps = std::max(2, static_cast<int>(std::ceil(static_cast<float>(kSegments) * value)));
    const ImU32 shade = ImGuiConverter::toImU32(context.getColor(Theme::Color::Overlay));
    ImDrawList& list = *ImGui::GetWindowDrawList();
    const ImVec2 middle = ImGuiConverter::toImVec2(center);
    for (int step = 0; step < steps; ++step) {
        const float from = start + kTwoPi * value * static_cast<float>(step) / static_cast<float>(steps);
        const float to = start + kTwoPi * value * static_cast<float>(step + 1) / static_cast<float>(steps);
        list.AddTriangleFilled(middle, {center.x + std::cos(from) * radius, center.y + std::sin(from) * radius}, {center.x + std::cos(to) * radius, center.y + std::sin(to) * radius}, shade);
    }
}

} // namespace haylen::ui
