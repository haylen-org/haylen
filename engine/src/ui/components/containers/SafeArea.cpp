#include "ui/components/containers/SafeArea.hpp"

#include "haylen/ui/Backend.hpp"
#include "haylen/ui/Context.hpp"

namespace haylen::ui {

math::Vec2 SafeArea::measureContent(Context& context, float availableWidth) {
    auto visible = getLayoutChildren();
    return visible.empty() ? math::Vec2{} : visible.front()->measure(context, availableWidth);
}

void SafeArea::render(Context& context, const math::Rect& bounds) {
    if (auto visible = getLayoutChildren(); !visible.empty()) {
        visible.front()->draw(context, bounds.intersection(context.getBackend().getSafeRect()));
    }
}

} // namespace haylen::ui
