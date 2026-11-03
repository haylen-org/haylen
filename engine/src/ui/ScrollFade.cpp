#include "ui/ScrollFade.hpp"

#include <algorithm>

#include "haylen/ui/Backend.hpp"
#include "haylen/ui/Context.hpp"

namespace haylen::ui {

// Each edge fades as strongly as the share of the length of the fade that the content continues past it.
bool ScrollFade::begin(Context& context, const math::Rect& box, bool horizontal, double before, double after) {
    const float length = std::min(context.getMetric(Theme::Metric::ScrollFade), (horizontal ? box.width : box.height) * 0.5F);
    if (length <= 0.0F) {
        return false;
    }

    const float leading = std::clamp(static_cast<float>(before) / length, 0.0F, 1.0F);
    const float trailing = std::clamp(static_cast<float>(after) / length, 0.0F, 1.0F);
    if (leading <= 0.0F && trailing <= 0.0F) {
        return false;
    }

    graphics2d::Renderer::Fade edges{.length = length};
    (horizontal ? edges.left : edges.top) = leading;
    (horizontal ? edges.right : edges.bottom) = trailing;
    context.getBackend().pushFade({.area = box, .edges = edges});
    return true;
}

void ScrollFade::end(Context& context) {
    context.getBackend().popFade();
}

} // namespace haylen::ui
