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
    math::Vec2 size;
    for (Component* child : visible) {
        const math::Vec2 measured = child->measure(context, inner);
        if (horizontal) {
            size.x += measured.x;
            size.y = std::max(size.y, measured.y);
        } else {
            size.x = std::max(size.x, measured.x);
            size.y += measured.y;
        }
    }
    const float gaps = visible.empty() ? 0.0F : getGap(context) * static_cast<float>(visible.size() - 1);
    (horizontal ? size.x : size.y) += gaps;
    return {size.x + insets.getHorizontal(), size.y + insets.getVertical()};
}

void Linear::render(Context& context, const math::Rect& bounds) {
    paint(context, bounds);
    const math::Rect inner = bounds.inset(getPadding(context));
    const std::vector<Component*> visible = getLayoutChildren();
    if (visible.empty()) {
        return;
    }

    // In a row, children that grow share what the others leave instead of adding to their own width, so wide content never pushes the row past its bounds.
    std::vector<math::Vec2> sizes;
    float total = getGap(context) * static_cast<float>(visible.size() - 1);
    float growth = 0.0F;
    for (Component* child : visible) {
        const bool shares = horizontal && child->getCommon().grow > 0.0F;
        sizes.push_back(shares ? math::Vec2{} : child->measure(context, inner.width));
        total += horizontal ? sizes.back().x : sizes.back().y;
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

    for (std::size_t index = 0; index < visible.size(); ++index) {
        Component& child = *visible[index];
        const float grown = growth > 0.0F ? extra * child.getCommon().grow / growth : 0.0F;
        if (horizontal) {
            const float width = sizes[index].x + grown;
            const Alignment alignment = child.getRowAlignment();
            const float height = alignment == Alignment::Stretch ? inner.height : std::min(child.measure(context, width).y, inner.height);
            child.draw(context, {std::floor(cursor), std::floor(align(alignment, inner.y, inner.height, height)), width, height});
            cursor += width + spacing;
        } else {
            const float height = sizes[index].y + grown;
            const Alignment alignment = child.getAlignment();
            const float width = alignment == Alignment::Stretch ? inner.width : std::min(sizes[index].x, inner.width);
            child.draw(context, {std::floor(align(alignment, inner.x, inner.width, width)), std::floor(cursor), width, height});
            cursor += height + spacing;
        }
    }
}

} // namespace haylen::ui
