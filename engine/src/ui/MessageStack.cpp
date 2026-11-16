#include "haylen/ui/MessageStack.hpp"

#include <algorithm>
#include <utility>

#include <imgui_internal.h>

namespace haylen::ui {

// The windows of the messages of the last frame come to the front from the deepest to the front one, so every message draws over those behind it.
void MessageStack::beginFrame() {
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

bool MessageStack::isBefore(const Entry& entry, const Request& request) noexcept {
    if (entry.shown != request.shown) {
        return entry.shown;
    }

    if (!entry.shown && entry.priority != request.priority) {
        return entry.priority > request.priority;
    }

    return entry.since < request.since || (entry.since == request.since && entry.key < request.key);
}

// The other messages of the position count first: those of the last frame, and those that asked to show since then and already placed themselves this frame. A list counts the messages before this one, and a deck the ones after it, which lie in front of it. A message waits behind the messages of its style before it, and only those that found room take room.
std::optional<MessageStack::Placement> MessageStack::place(const Request& request) {
    const bool deck = request.arrangement == Arrangement::Deck;
    std::size_t alike = 0;
    std::size_t ahead = 0;
    float offset = 0.0F;
    double newest = request.since;
    float front = request.extent;

    // clang-format off
    const auto count = [&](const Entry& entry) {
        if (entry.position != request.position || entry.key == request.key || isBefore(entry, request) == deck) {
            return;
        }

        if (entry.style == request.style) {
            ++alike;
        }

        if (!entry.placed) {
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

    const bool open = request.expandable && isHeld(request.position);
    const bool waits = !deck && request.limit > 0 && alike >= request.limit;
    const Placement placement = !deck || open ? Placement{.offset = offset, .depth = 0, .front = 0.0F} : Placement{.offset = static_cast<float>(ahead) * request.peek, .depth = ahead, .front = front};
    current.push_back({.key = request.key, .style = request.style, .position = request.position, .extent = request.extent, .since = request.since, .priority = request.priority, .shown = request.shown, .placed = !waits, .depth = placement.depth});

    if (waits) {
        return std::nullopt;
    }

    return placement;
}

void MessageStack::attach(std::uint64_t key, ImGuiID window) noexcept {
    for (Entry& entry : current) {
        if (entry.key == key) {
            entry.window = window;
        }
    }
}

void MessageStack::hold(Position position) noexcept {
    holding[static_cast<std::size_t>(position)] = true;
}

bool MessageStack::isHeld(Position position) const noexcept {
    return held[static_cast<std::size_t>(position)];
}

void MessageStack::cover(Position position, const math::Rect& frame) noexcept {
    math::Rect& area = covering[static_cast<std::size_t>(position)];
    area = area.isEmpty() ? frame : math::Rect::fromMinMax(math::Vec2::min(area.getMin(), frame.getMin()), math::Vec2::max(area.getMax(), frame.getMax()));
}

const math::Rect& MessageStack::getArea(Position position) const noexcept {
    return areas[static_cast<std::size_t>(position)];
}

} // namespace haylen::ui
