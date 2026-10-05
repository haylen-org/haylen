#include "ui/components/inputs/RangeSlider.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include "haylen/ui/Context.hpp"
#include "haylen/ui/FocusNavigator.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/Surfaces.hpp"
#include "ui/Typography.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

void RangeSlider::readProperties(PropertyReader& reader) {
    double lowest = minimum;
    double highest = maximum;
    double from = low;
    double to = high;
    reader.read("low", from);
    reader.read("high", to);
    reader.read("min", lowest, -Widgets::kSliderLimit, Widgets::kSliderLimit);
    reader.read("max", highest, -Widgets::kSliderLimit, Widgets::kSliderLimit);
    reader.read("step", step, 0.0);
    reader.read("showValue", showValue);
    reader.read("decimals", decimals, 0, 6);
    if (lowest >= highest) {
        reader.fail("min", "must be smaller than \"max\"");
    }
    if (reader.has("low") && reader.has("high") && from > to) {
        reader.fail("low", "must not be greater than \"high\"");
    }

    // An end given alone that passes the other end, such as where the player left it, takes that end along.
    minimum = lowest;
    maximum = highest;
    low = std::clamp(from, minimum, maximum);
    high = std::clamp(to, minimum, maximum);
    if (low > high && !reader.has("low")) {
        low = high;
    }
    high = std::max(low, high);
}

math::Vec2 RangeSlider::measureContent(Context& context, float availableWidth) {
    return {std::min(availableWidth, 420.0F), context.getMetric(Theme::Metric::ControlHeight)};
}

void RangeSlider::render(Context& context, const math::Rect& bounds) {
    const std::string range = Typography::formatNumber(low, decimals) + " - " + Typography::formatNumber(high, decimals);
    const float label = showValue ? measureLabel(context) : 0.0F;
    const math::Rect area = context.mirror({bounds.x, bounds.y, std::max(0.0F, bounds.width - label), bounds.height}, bounds);
    const float knob = context.getMetric(Theme::Metric::SliderKnobSize);
    const float trackHeight = context.getMetric(Theme::Metric::SliderTrackHeight);
    const math::Rect track{area.x + knob * 0.5F, std::floor(area.getCenter().y - trackHeight * 0.5F), std::max(0.0F, area.width - knob), trackHeight};

    bool changed = follow(context, area, track);
    const ImGuiID itemId = ImGui::GetItemID();
    if (takeFocusRequest()) {
        Widgets::focusItem(context);
    }
    if (GImGui->NavActivatePressedId == itemId) {
        highActive = !highActive;
    }
    if (const std::optional<FocusDirection> direction = takeFocusDirection(context)) {
        const double amount = (step > 0.0 ? step : (maximum - minimum) / kFocusSteps) * Widgets::getStep(context, *direction);
        double& moved = highActive ? high : low;
        const double next = std::clamp(Widgets::snap(moved + amount, minimum, maximum, step), highActive ? low : minimum, highActive ? maximum : high);
        changed = changed || next != moved;
        moved = next;
    }

    const math::Rect groove = track.inset(Surfaces::getPadding(context, Theme::Surface::Track));
    Surfaces::draw(context, Theme::Surface::Track, track, context.getColor(Theme::Color::BorderStrong), std::nullopt, trackHeight * 0.5F);
    const float from = toPosition(context, low, groove);
    const float to = toPosition(context, high, groove);
    Surfaces::draw(context, Theme::Surface::TrackFill, {std::min(from, to), groove.y, std::fabs(to - from), groove.height}, context.getColor(Theme::Color::Accent), std::nullopt, groove.height * 0.5F);
    const bool ring = context.getFocus().isRingShown(itemId);
    const bool pressed = GImGui->ActiveId == itemId;
    drawKnob(context, area, toPosition(context, low, track), ring && !highActive, pressed && !highActive);
    drawKnob(context, area, toPosition(context, high, track), ring && highActive, pressed && highActive);
    if (showValue) {
        Typography::drawAligned(context, Theme::Font::Body, context.mirror({bounds.getRight() - label, bounds.y, label, bounds.height}, bounds), context.getColor(Theme::Color::TextMuted), range, Alignment::End);
    }
    if (changed) {
        context.emit(*this, "change", {{"low", low}, {"high", high}});
    }
}

// The shown range takes the room of its widest text, with both ends as wide as the wider end of the track, so the track keeps its length while the knobs move.
float RangeSlider::measureLabel(Context& context) const {
    const std::string first = Typography::formatNumber(minimum, decimals);
    const std::string last = Typography::formatNumber(maximum, decimals);
    const std::string& wider = Typography::measure(context, Theme::Font::Body, first).x > Typography::measure(context, Theme::Font::Body, last).x ? first : last;
    return Typography::measure(context, Theme::Font::Body, wider + " - " + wider).x + context.getMetric(Theme::Metric::ItemSpacing);
}

// The range grows toward the end of the UI, the left of a right-to-left UI.
float RangeSlider::toPosition(const Context& context, double amount, const math::Rect& track) const noexcept {
    const float along = track.width * static_cast<float>((amount - minimum) / (maximum - minimum));
    return context.isRightToLeft() ? track.getRight() - along : track.x + along;
}

// The pointer grabs the knob nearer to where it presses and drags it up to the other knob, the way ImGui's own slider follows the mouse and touch.
bool RangeSlider::follow(Context& context, const math::Rect& bounds, const math::Rect& track) {
    ImGuiContext& state = *GImGui;
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const ImGuiID itemId = ImGui::GetID("##range");
    const ImRect box = ImGuiConverter::toImRect(bounds);
    const bool shown = ImGui::ItemAdd(box, itemId);
    context.getFocus().addTarget(itemId, bounds);
    if (!shown) {
        return false;
    }

    const float pointer = state.IO.MousePos.x;
    if (ImGui::ItemHoverable(box, itemId, state.LastItemData.ItemFlags) && ImGui::IsMouseClicked(0, ImGuiInputFlags_None, itemId)) {
        ImGui::SetKeyOwner(ImGuiKey_MouseLeft, itemId);
        ImGui::SetActiveID(itemId, window);
        if (context.getFocus().isFocusable()) {
            ImGui::SetFocusID(itemId, window);
        }
        ImGui::FocusWindow(window);
        const float highX = toPosition(context, high, track);
        const float toLow = std::fabs(pointer - toPosition(context, low, track));
        const float toHigh = std::fabs(pointer - highX);
        highActive = toHigh < toLow || (toHigh == toLow && (context.isRightToLeft() ? pointer < highX : pointer > highX));
    }
    Widgets::drawFocusRing(context, bounds, itemId, bounds.height * 0.5F);
    if (state.ActiveId != itemId) {
        return false;
    }
    if (!state.IO.MouseDown[0]) {
        ImGui::ClearActiveID();
        return false;
    }

    const float along = context.isRightToLeft() ? track.getRight() - pointer : pointer - track.x;
    const double picked = Widgets::snap(minimum + static_cast<double>(std::clamp(along / std::max(1.0F, track.width), 0.0F, 1.0F)) * (maximum - minimum), minimum, maximum, step);
    double& moved = highActive ? high : low;
    const double next = std::clamp(picked, highActive ? low : minimum, highActive ? maximum : high);
    const bool changed = next != moved;
    moved = next;
    return changed;
}

void RangeSlider::drawKnob(Context& context, const math::Rect& bounds, float x, bool active, bool pressed) const {
    const float knob = context.getMetric(Theme::Metric::SliderKnobSize);
    const math::Rect area{x - knob * 0.5F, std::floor(bounds.getCenter().y - knob * 0.5F), knob, knob};
    const math::Color fill = context.getColor(Theme::Color::OnAccent);
    Surfaces::draw(context, Theme::Surface::Knob, area, pressed ? math::Color{fill.r * 0.85F, fill.g * 0.85F, fill.b * 0.85F, fill.a} : fill, context.getColor(Theme::Color::Accent), knob * 0.5F);
    if (active) {
        const float width = context.getMetric(Theme::Metric::FocusWidth);
        const float outer = knob * 0.5F + 4.0F + width * 0.5F;
        Surfaces::outline(context, math::Rect::fromCenter(area.getCenter(), {outer * 2.0F, outer * 2.0F}), context.getColor(Theme::Color::Focus), outer, width);
    }
}

} // namespace haylen::ui
