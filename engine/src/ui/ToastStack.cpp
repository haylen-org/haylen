#include "haylen/ui/ToastStack.hpp"

#include <algorithm>
#include <utility>

#include <imgui_internal.h>

namespace haylen::ui {

// The windows of the notices of the last frame come to the front from the deepest to the front one, so every notice draws over those behind it.
void ToastStack::beginFrame() {
    order.clear();
    for (const Entry& entry : current) {
        if (entry.window != 0) {
            order.push_back(&entry);
        }
    }
    std::ranges::sort(order, [](const Entry* first, const Entry* second) { return first->depth != second->depth ? first->depth > second->depth : first->key < second->key; });
    for (const Entry* entry : order) {
        if (ImGuiWindow* window = ImGui::FindWindowByID(entry->window)) {
            ImGui::BringWindowToDisplayFront(window);
        }
    }

    std::swap(previous, current);
    current.clear();
    held = std::exchange(holding, {});
    areas = std::exchange(covering, {});
}

bool ToastStack::isBefore(const Entry& entry, std::uint64_t key, double since) noexcept {
    return entry.since < since || (entry.since == since && entry.key < key);
}

// The other notices of the position count first: those of the last frame, and those that asked to show since then and already placed themselves this frame. A list counts the notices that asked before this one, and a deck the ones that asked after it, which lie in front of it.
std::optional<ToastStack::Placement> ToastStack::place(std::uint64_t key, Position position, Arrangement arrangement, float extent, double since, std::size_t limit, float peek) {
    const bool deck = arrangement == Arrangement::Deck;
    std::size_t ahead = 0;
    float offset = 0.0F;
    double newest = since;
    float front = extent;
    // clang-format off
    const auto count = [&](const Entry& entry) {
        if (entry.position != position || entry.key == key || isBefore(entry, key, since) == deck) {
            return;
        }
        ++ahead;
        offset += entry.extent;
        if (entry.since >= newest) {
            newest = entry.since;
            front = entry.extent;
        }
    };
    // clang-format on
    for (const Entry& entry : previous) {
        count(entry);
    }
    for (const Entry& entry : current) {
        if (std::ranges::none_of(previous, [&entry](const Entry& other) { return other.key == entry.key; })) {
            count(entry);
        }
    }

    const bool open = isHeld(position);
    const Placement placement = !deck || open ? Placement{.offset = offset, .depth = 0, .front = 0.0F} : Placement{.offset = static_cast<float>(ahead) * peek, .depth = ahead, .front = front};
    current.push_back({.key = key, .position = position, .extent = extent, .since = since, .depth = placement.depth});
    if (!deck && limit > 0 && ahead >= limit) {
        return std::nullopt;
    }
    return placement;
}

void ToastStack::attach(std::uint64_t key, ImGuiID window) noexcept {
    for (Entry& entry : current) {
        if (entry.key == key) {
            entry.window = window;
        }
    }
}

void ToastStack::hold(Position position) noexcept {
    holding[static_cast<std::size_t>(position)] = true;
}

bool ToastStack::isHeld(Position position) const noexcept {
    return held[static_cast<std::size_t>(position)];
}

void ToastStack::cover(Position position, const math::Rect& frame) noexcept {
    math::Rect& area = covering[static_cast<std::size_t>(position)];
    area = area.isEmpty() ? frame : math::Rect::fromMinMax(math::Vec2::min(area.getMin(), frame.getMin()), math::Vec2::max(area.getMax(), frame.getMax()));
}

const math::Rect& ToastStack::getArea(Position position) const noexcept {
    return areas[static_cast<std::size_t>(position)];
}

} // namespace haylen::ui
