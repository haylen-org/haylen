#include "ui/components/overlays/Toast.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <optional>
#include <stdexcept>
#include <string>

#include <imgui.h>

#include "haylen/core/JsonValidator.hpp"
#include "haylen/ui/Backend.hpp"
#include "haylen/ui/Context.hpp"
#include "haylen/ui/PropertyReader.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/Surfaces.hpp"
#include "ui/Typography.hpp"

namespace haylen::ui {

void Toast::readProperties(PropertyReader& reader) {
    reader.read("text", text);
    reader.readChoice<Widgets::Tone>("tone", tone, Widgets::kTones);
    reader.read("duration", duration, 0.0F, 3600.0F);
    reader.readChoice<ToastStack::Position>("position", position, ToastStack::kPositions);
    if (reader.has("open")) {
        reader.read("open", open);
        restart();
    }
}

// Setting `open` shows the notice of the properties from the start, as a new notice at the end of the stack, and clearing it lets the notice leave.
void Toast::restart() {
    for (Notice& notice : notices) {
        notice.leaving = notice.leaving || notice.own;
    }
    if (open) {
        notices.push_back({.text = {}, .tone = tone, .duration = duration, .own = true});
    }
}

void Toast::command(Context& context, std::string_view name, const core::Json& arguments) {
    if (name != "show") {
        Component::command(context, name, arguments);
        return;
    }
    if (!arguments.is_object()) {
        throw std::invalid_argument("The \"show\" command of a \"toast\" takes a table with \"text\", \"tone\" and \"duration\".");
    }
    core::JsonValidator::requireKnownKeys(arguments, {"text", "tone", "duration"}, "the \"show\" command of a \"toast\"");
    PropertyReader reader(arguments, getKind());
    Notice notice{.tone = tone, .duration = duration};
    reader.read("text", notice.text);
    reader.readChoice<Widgets::Tone>("tone", notice.tone, Widgets::kTones);
    reader.read("duration", notice.duration, 0.0F, 3600.0F);
    reader.finish();
    notices.push_back(std::move(notice));
}

math::Vec2 Toast::measureContent(Context&, float) {
    return {};
}

void Toast::render(Context& context, const math::Rect&) {
    std::size_t index = 0;
    while (index < notices.size()) {
        if (drawNotice(context, notices[index])) {
            if (notices[index].own) {
                open = false;
                context.emit(*this, "dismiss");
            }
            notices.erase(notices.begin() + static_cast<std::ptrdiff_t>(index));
            continue;
        }
        ++index;
    }
}

// A notice waits while the stack is full, and its time runs from the frame it first shows. It slides in from the edge of its stack and fades in, settles into its place as the notices before it leave, and fades out while its height in the stack shrinks, so the notices after it move up.
bool Toast::drawNotice(Context& context, Notice& notice) {
    ToastStack& stack = context.getToasts();
    if (notice.key == 0) {
        notice.key = stack.createKey();
    }
    if (notice.since < 0.0) {
        notice.since = context.getTime();
    }
    const math::Rect safe = context.getBackend().getSafeRect();
    const float margin = context.getMetric(Theme::Metric::PanelPadding);
    const float width = std::max(0.0F, std::min(context.getMetric(Theme::Metric::ToastWidth), safe.width - margin * 2.0F));
    const float padding = context.getMetric(Theme::Metric::ControlPaddingX);
    const float bar = context.getMetric(Theme::Metric::ToneBarWidth);
    const std::string message = context.getText(notice.own ? text : notice.text);
    const float height = Typography::measureParagraph(context, Theme::Font::Body, message, width - padding * 2.0F - bar).y + context.getMetric(Theme::Metric::ControlPaddingY) * 2.0F;

    // A notice that comes in takes its whole place at once, and one that leaves gives its place back as it fades.
    const float extent = (height + context.getMetric(Theme::Metric::ItemSpacing)) * (notice.leaving ? notice.appear : 1.0F);
    const std::optional<float> place = stack.place(notice.key, position, extent, notice.since, static_cast<std::size_t>(context.getMetric(Theme::Metric::ToastLimit)));
    if (!place) {
        return false;
    }
    if (notice.shownAt < 0.0) {
        notice.shownAt = context.getTime();
        notice.offset = *place;
    }

    const float length = notice.own ? duration : notice.duration;
    if (length > 0.0F && context.getTime() - notice.shownAt >= length) {
        notice.leaving = true;
    }
    const float transition = context.getMetric(Theme::Metric::TransitionDuration);
    const float step = transition > 0.0F ? context.getDeltaSeconds() / transition : 1.0F;
    notice.appear = notice.leaving ? std::max(0.0F, notice.appear - step) : std::min(1.0F, notice.appear + step);
    notice.offset += (*place - notice.offset) * std::min(1.0F, step * 3.0F);
    if (notice.leaving && notice.appear <= 0.0F) {
        return true;
    }

    // Start and end stacks line up with the sides of the safe area the UI starts and ends at, and a notice comes in from the edge of its stack.
    const bool bottom = ToastStack::isBottom(position);
    const bool start = position == ToastStack::Position::TopStart || position == ToastStack::Position::BottomStart;
    const bool end = position == ToastStack::Position::TopEnd || position == ToastStack::Position::BottomEnd;
    float x = safe.getCenter().x - width * 0.5F;
    if (start || end) {
        x = (start != context.isRightToLeft()) ? safe.x + margin : safe.getRight() - margin - width;
    }
    const float slide = (1.0F - notice.appear) * margin;
    const float y = bottom ? safe.getBottom() - margin - notice.offset - height + slide : safe.y + margin + notice.offset - slide;
    const math::Rect frame{std::floor(x), std::floor(y), width, height};
    setBounds(frame);

    std::array<char, 32> name{};
    std::snprintf(name.data(), name.size(), "##toast%llu", static_cast<unsigned long long>(notice.key));
    ImGui::SetNextWindowPos(ImGuiConverter::toImVec2(frame.getMin()));
    ImGui::SetNextWindowSize(ImGuiConverter::toImVec2(frame.getSize()));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * notice.appear);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin(name.data(), nullptr, flags)) {
        drawFrame(context, notice, frame, message);
    }
    ImGui::End();
    ImGui::PopStyleVar();
    return false;
}

// The bar of the tone fills the start side of the frame, clipped to its width, so it follows the rounded corners of the frame.
void Toast::drawFrame(Context& context, const Notice& notice, const math::Rect& frame, const std::string& message) {
    const Widgets::ToneColors colors = Widgets::getToneColors(notice.own ? tone : notice.tone);
    const float barWidth = context.getMetric(Theme::Metric::ToneBarWidth);
    const float radius = context.getMetric(Theme::Metric::PanelRadius);
    Surfaces::drawShadow(context, frame, radius);
    Surfaces::draw(context, Theme::Surface::Toast, frame, context.getColor(Theme::Color::Tooltip), std::nullopt, radius);
    const math::Rect bar = context.mirror({frame.x, frame.y, barWidth, frame.height}, frame);
    ImDrawList& list = *ImGui::GetWindowDrawList();
    list.PushClipRect(ImGuiConverter::toImVec2(bar.getMin()), ImGuiConverter::toImVec2(bar.getMax()), true);
    Surfaces::fill(context, frame, context.getColor(colors.fill), radius);
    list.PopClipRect();

    const float padding = context.getMetric(Theme::Metric::ControlPaddingX);
    const math::Rect inner = context.mirror(math::Rect::fromMinMax({frame.x + barWidth + padding, frame.y + context.getMetric(Theme::Metric::ControlPaddingY)}, {frame.getRight() - padding, frame.getBottom()}), frame);
    Typography::drawParagraph(context, Theme::Font::Body, inner, context.getColor(Theme::Color::OnTooltip), message, Alignment::Start);
}

} // namespace haylen::ui
