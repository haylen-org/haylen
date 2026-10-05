#include "ui/components/containers/Linear.hpp"

#include <algorithm>
#include <cmath>

#include "haylen/math/Insets.hpp"
#include "haylen/ui/Context.hpp"

namespace haylen::ui {

void Linear::readProperties(PropertyReader& reader) {
    if (reader.has("gap")) {
        float value = 0.0F;
        reader.read("gap", value, 0.0F, 10000.0F);
        gap = value;
    }
    reader.read("padding", padding);
    reader.readChoice<Justify>("justify", justify, kJustify);
    if (reader.has("alignItems")) {
        Alignment value = Alignment::Start;
        reader.readChoice<Alignment>("alignItems", value, kAlignments);
        alignItems = value;
    }

    // Only rows wrap, since a column has no height to wrap at until it is drawn.
    if (horizontal) {
        reader.read("wrap", wrap);
        if (reader.has("lineGap")) {
            float value = 0.0F;
            reader.read("lineGap", value, 0.0F, 10000.0F);
            lineGap = value;
        }
    }
    readMore(reader);
}

math::Insets Linear::getPadding(Context&) const {
    return padding;
}

float Linear::getGap(Context& context) const {
    return gap.value_or(context.getMetric(Theme::Metric::ItemSpacing));
}

float Linear::getLineGap(Context& context) const {
    return lineGap.value_or(getGap(context));
}

Alignment Linear::getChildAlignment(const Component& child) const noexcept {
    if (child.getCommon().align) {
        return *child.getCommon().align;
    }
    if (alignItems) {
        return *alignItems;
    }
    return horizontal ? child.getRowAlignment() : child.getAlignment();
}

float Linear::measureLength(Context& context, Component& child, float crossLength) const {
    const math::Vec2 measured = child.measure(context, crossLength);
    return horizontal ? measured.x : measured.y;
}

float Linear::clampLength(const Component& child, float length) const noexcept {
    return horizontal ? child.clampWidth(length) : child.clampHeight(length);
}

math::Vec2 Linear::measureContent(Context& context, float availableWidth) {
    const math::Insets insets = getPadding(context);
    const float inner = std::max(0.0F, availableWidth - insets.getHorizontal());
    if (horizontal && wrap) {
        const math::Vec2 wrapped = measureWrapped(context, inner);
        return {wrapped.x + insets.getHorizontal(), wrapped.y + insets.getVertical()};
    }

    auto visible = getLayoutChildren();
    const auto count = static_cast<float>(std::ranges::distance(visible));
    float length = count > 0.0F ? getGap(context) * (count - 1.0F) : 0.0F;
    float breadth = 0.0F;
    float growth = 0.0F;
    for (Component* child : visible) {
        if (child->getCommon().grow > 0.0F) {
            growth += child->getCommon().grow;
            continue;
        }
        const math::Vec2 measured = child->measure(context, inner);
        length += horizontal ? measured.x : measured.y;
        breadth = std::max(breadth, horizontal ? measured.y : measured.x);
    }

    // Growing children measure last. A row gives each one its share of the width the others leave, the width it draws at, so text that wraps there reports every line. They grow from nothing when they draw, so a container of their own size takes the room whose split by grow factors still fits each one.
    const float free = std::max(0.0F, inner - length);
    float share = 0.0F;
    for (Component* child : visible) {
        const float factor = child->getCommon().grow;
        if (factor <= 0.0F) {
            continue;
        }
        const float width = horizontal ? free * factor / growth : inner;
        const math::Vec2 measured = child->measure(context, width);
        share = std::max(share, (horizontal ? measured.x : measured.y) / factor);
        breadth = std::max(breadth, horizontal ? measured.y : measured.x);
    }
    length += share * growth;
    return horizontal ? math::Vec2{length + insets.getHorizontal(), breadth + insets.getVertical()} : math::Vec2{breadth + insets.getHorizontal(), length + insets.getVertical()};
}

// Lines take children while they fit at their natural width, and a child wider than the whole row takes a line of its own.
math::Vec2 Linear::measureWrapped(Context& context, float inner) {
    breakLines(context, inner);
    const float spacing = getGap(context);
    float width = 0.0F;
    float height = 0.0F;
    for (std::size_t line = 0; line < lineStarts.size(); ++line) {
        const std::size_t first = lineStarts[line];
        const std::size_t last = line + 1 < lineStarts.size() ? lineStarts[line + 1] : slots.size();
        float used = spacing * static_cast<float>(last - first - 1);
        float tallest = 0.0F;
        for (std::size_t index = first; index < last; ++index) {
            used += slots[index].length;
            tallest = std::max(tallest, slots[index].child->measure(context, slots[index].length).y);
        }
        width = std::max(width, used);
        height += tallest + (line > 0 ? getLineGap(context) : 0.0F);
    }
    return {width, height};
}

void Linear::breakLines(Context& context, float inner) {
    slots.clear();
    lineStarts.clear();
    const float spacing = getGap(context);
    float used = 0.0F;
    for (Component* child : getLayoutChildren()) {
        const float length = child->measure(context, inner).x;
        if (slots.empty() || used + spacing + length > inner) {
            lineStarts.push_back(slots.size());
            used = length;
        } else {
            used += spacing + length;
        }
        slots.push_back({.child = child, .base = length, .length = length, .growing = child->getCommon().grow > 0.0F});
    }
}

// Growing children share the free length by their factors from the length they start from, and a child that its minimum or maximum size stops keeps that size and leaves the rest to the others.
void Linear::grow(std::size_t first, std::size_t last, float available, float spacing) {
    while (true) {
        float used = spacing * static_cast<float>(last - first - 1);
        float growth = 0.0F;
        for (std::size_t index = first; index < last; ++index) {
            const Slot& slot = slots[index];
            used += slot.growing ? slot.base : slot.length;
            growth += slot.growing ? slot.child->getCommon().grow : 0.0F;
        }
        if (growth <= 0.0F) {
            return;
        }

        const float free = std::max(0.0F, available - used);
        bool bounded = false;
        for (std::size_t index = first; index < last; ++index) {
            Slot& slot = slots[index];
            if (!slot.growing) {
                continue;
            }
            slot.length = slot.base + free * slot.child->getCommon().grow / growth;
            const float allowed = clampLength(*slot.child, slot.length);
            if (allowed != slot.length) {
                slot.length = allowed;
                slot.growing = false;
                bounded = true;
            }
        }
        if (!bounded) {
            return;
        }
    }
}

// The free length that growing children leave goes to `justify`, and children start at the nearest whole unit along the main axis, so spaces that do not divide evenly never drift by a unit. A row runs from the right in a right-to-left UI, while start and end across a column follow the direction.
void Linear::place(Context& context, const math::Rect& line, std::size_t first, std::size_t last, float spacing) {
    const auto count = static_cast<float>(last - first);
    float used = spacing * (count - 1.0F);
    for (std::size_t index = first; index < last; ++index) {
        used += slots[index].length;
    }
    const float extra = std::max(0.0F, (horizontal ? line.width : line.height) - used);
    float cursor = horizontal ? line.x : line.y;
    switch (justify) {
    case Justify::Center:
        cursor += extra * 0.5F;
        break;
    case Justify::End:
        cursor += extra;
        break;
    case Justify::SpaceBetween:
        spacing += count > 1.0F ? extra / (count - 1.0F) : 0.0F;
        break;
    case Justify::SpaceAround:
        cursor += extra / count * 0.5F;
        spacing += extra / count;
        break;
    case Justify::SpaceEvenly:
        cursor += extra / (count + 1.0F);
        spacing += extra / (count + 1.0F);
        break;
    case Justify::Start:
        break;
    }

    for (std::size_t index = first; index < last; ++index) {
        Component& child = *slots[index].child;
        const float length = slots[index].length;
        const Alignment alignment = getChildAlignment(child);
        if (horizontal) {
            const float height = alignment == Alignment::Stretch ? std::min(child.clampHeight(line.height), line.height) : std::min(child.measure(context, length).y, line.height);
            const math::Rect placed = context.mirror({std::round(cursor), align(alignment, line.y, line.height, height), length, height}, line);
            child.draw(context, {std::round(placed.x), std::floor(placed.y), length, height});
        } else {
            const float width = alignment == Alignment::Stretch ? std::min(child.clampWidth(line.width), line.width) : std::min(child.measure(context, line.width).x, line.width);
            child.draw(context, {std::floor(context.alignHorizontally(alignment, line.x, line.width, width)), std::round(cursor), width, length});
        }
        cursor += length + spacing;
    }
}

void Linear::render(Context& context, const math::Rect& bounds) {
    paint(context, bounds);
    const math::Rect inner = bounds.inset(getPadding(context));
    const float spacing = getGap(context);
    if (horizontal && wrap) {
        breakLines(context, inner.width);
        float y = inner.y;
        for (std::size_t line = 0; line < lineStarts.size(); ++line) {
            const std::size_t first = lineStarts[line];
            const std::size_t last = line + 1 < lineStarts.size() ? lineStarts[line + 1] : slots.size();
            grow(first, last, inner.width, spacing);
            float height = 0.0F;
            for (std::size_t index = first; index < last; ++index) {
                height = std::max(height, slots[index].child->measure(context, slots[index].length).y);
            }
            place(context, {inner.x, y, inner.width, height}, first, last, spacing);
            y += height + getLineGap(context);
        }
        return;
    }

    // Children that grow start from nothing but their margin and share the room the others leave along the main axis, so their content never pushes the container past its bounds, and a scroll that grows scrolls inside that room.
    slots.clear();
    for (Component* child : getLayoutChildren()) {
        const bool growing = child->getCommon().grow > 0.0F;
        const float margin = horizontal ? child->getCommon().margin.getHorizontal() : child->getCommon().margin.getVertical();
        const float length = growing ? margin : measureLength(context, *child, inner.width);
        slots.push_back({.child = child, .base = length, .length = length, .growing = growing});
    }
    if (slots.empty()) {
        return;
    }
    grow(0, slots.size(), horizontal ? inner.width : inner.height, spacing);
    place(context, inner, 0, slots.size(), spacing);
}

} // namespace haylen::ui
