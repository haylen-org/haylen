#include "ui/components/containers/Stack.hpp"

#include <algorithm>
#include <cmath>

#include "haylen/ui/Context.hpp"

namespace haylen::ui {

void Stack::readProperties(PropertyReader& reader) {
    reader.read("padding", padding);
    if (reader.has("alignItems")) {
        Alignment value = Alignment::Start;
        reader.readChoice<Alignment>("alignItems", value, kAlignments);
        alignItems = value;
    }
}

math::Vec2 Stack::measureContent(Context& context, float availableWidth) {
    math::Vec2 size;
    for (Component* child : getLayoutChildren()) {
        size = math::Vec2::max(size, child->measure(context, std::max(0.0F, availableWidth - padding.getHorizontal())));
    }
    return {size.x + padding.getHorizontal(), size.y + padding.getVertical()};
}

// Every child sits by its own alignment in both directions, or by `alignItems` when it sets none, and a stretched child stays within its size bounds.
void Stack::render(Context& context, const math::Rect& bounds) {
    const math::Rect inner = bounds.inset(padding);
    for (Component* child : getLayoutChildren()) {
        const Alignment alignment = child->getCommon().align.value_or(alignItems.value_or(child->getAlignment()));
        if (alignment == Alignment::Stretch) {
            child->draw(context, {inner.x, inner.y, std::min(child->clampWidth(inner.width), inner.width), std::min(child->clampHeight(inner.height), inner.height)});
            continue;
        }
        const math::Vec2 fitted = math::Vec2::min(child->measure(context, inner.width), inner.getSize());
        child->draw(context, {std::floor(context.alignHorizontally(alignment, inner.x, inner.width, fitted.x)), std::floor(align(alignment, inner.y, inner.height, fitted.y)), fitted.x, fitted.y});
    }
}

} // namespace haylen::ui
