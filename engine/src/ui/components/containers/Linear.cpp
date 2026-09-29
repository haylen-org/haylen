#include "ui/components/containers/Linear.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

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
    readMore(reader);
}

math::Insets Linear::getPadding(Context&) const {
    return padding;
}

float Linear::getGap(Context& context) const {
    return gap.value_or(context.getMetric(Theme::Metric::ItemSpacing));
}

math::Vec2 Linear::measureContent(Context& context, float availableWidth) {
    const math::Insets insets = getPadding(context);
    const float inner = std::max(0.0F, availableWidth - insets.getHorizontal());
    const std::vector<Component*> visible = getLayoutChildren();
    float length = visible.empty() ? 0.0F : getGap(context) * static_cast<float>(visible.size() - 1);
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
        const float grow = child->getCommon().grow;
        if (grow <= 0.0F) {
            continue;
        }
        const float width = horizontal ? free * grow / growth : inner;
        const math::Vec2 measured = child->measure(context, width);
        share = std::max(share, (horizontal ? measured.x : measured.y) / grow);
        breadth = std::max(breadth, horizontal ? measured.y : measured.x);
    }
    length += share * growth;
    return horizontal ? math::Vec2{length + insets.getHorizontal(), breadth + insets.getVertical()} : math::Vec2{breadth + insets.getHorizontal(), length + insets.getVertical()};
}

void Linear::render(Context& context, const math::Rect& bounds) {
    paint(context, bounds);
    const math::Rect inner = bounds.inset(getPadding(context));
    const std::vector<Component*> visible = getLayoutChildren();
    if (visible.empty()) {
        return;
    }

    // Children that grow start from nothing and share the room the others leave along the main axis, so their content never pushes the container past its bounds, and a scroll that grows scrolls inside that room.
    std::vector<float> lengths;
    float total = getGap(context) * static_cast<float>(visible.size() - 1);
    float growth = 0.0F;
    for (Component* child : visible) {
        const math::Vec2 measured = child->getCommon().grow > 0.0F ? math::Vec2{} : child->measure(context, inner.width);
        lengths.push_back(horizontal ? measured.x : measured.y);
        total += lengths.back();
        growth += child->getCommon().grow;
    }

    const float available = horizontal ? inner.width : inner.height;
    const float extra = std::max(0.0F, available - total);
    float cursor = horizontal ? inner.x : inner.y;
    float spacing = getGap(context);
    if (growth <= 0.0F) {
        if (justify == Justify::Center) {
            cursor += extra * 0.5F;
        } else if (justify == Justify::End) {
            cursor += extra;
        } else if (justify == Justify::SpaceBetween && visible.size() > 1) {
            spacing += extra / static_cast<float>(visible.size() - 1);
        }
    }

    // A row runs from the right in a right-to-left UI, and start and end across a column follow the direction.
    for (std::size_t index = 0; index < visible.size(); ++index) {
        Component& child = *visible[index];
        const float grown = growth > 0.0F ? extra * child.getCommon().grow / growth : 0.0F;
        if (horizontal) {
            const float width = lengths[index] + grown;
            const Alignment alignment = child.getRowAlignment();
            const float height = alignment == Alignment::Stretch ? inner.height : std::min(child.measure(context, width).y, inner.height);
            const math::Rect placed = context.mirror({cursor, align(alignment, inner.y, inner.height, height), width, height}, inner);
            child.draw(context, {std::floor(placed.x), std::floor(placed.y), width, height});
            cursor += width + spacing;
        } else {
            const float height = lengths[index] + grown;
            const Alignment alignment = child.getAlignment();
            const float width = alignment == Alignment::Stretch ? inner.width : std::min(child.measure(context, inner.width).x, inner.width);
            child.draw(context, {std::floor(context.alignHorizontally(alignment, inner.x, inner.width, width)), std::floor(cursor), width, height});
            cursor += height + spacing;
        }
    }
}

} // namespace haylen::ui
