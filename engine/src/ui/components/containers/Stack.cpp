#include "ui/components/containers/Stack.hpp"

#include <algorithm>
#include <cmath>

#include "haylen/ui/Context.hpp"

namespace haylen::ui {

void Stack::readProperties(PropertyReader& reader) {
    reader.read("padding", padding);
}

math::Vec2 Stack::measureContent(Context& context, float availableWidth) {
    math::Vec2 size;
    for (Component* child : getLayoutChildren()) {
        size = math::Vec2::max(size, child->measure(context, std::max(0.0F, availableWidth - padding.getHorizontal())));
    }
    return {size.x + padding.getHorizontal(), size.y + padding.getVertical()};
}

void Stack::render(Context& context, const math::Rect& bounds) {
    const math::Rect inner = bounds.inset(padding);
    for (Component* child : getLayoutChildren()) {
        const math::Vec2 size = child->measure(context, inner.width);
        const Alignment alignment = child->getAlignment();
        if (alignment == Alignment::Stretch) {
            child->draw(context, inner);
            continue;
        }
        const math::Vec2 fitted = math::Vec2::min(size, inner.getSize());
        child->draw(context, {std::floor(context.alignHorizontally(alignment, inner.x, inner.width, fitted.x)), std::floor(align(alignment, inner.y, inner.height, fitted.y)), fitted.x, fitted.y});
    }
}

} // namespace haylen::ui
