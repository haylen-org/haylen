#include "ui/components/indicators/Icon.hpp"

#include <algorithm>

#include "haylen/ui/Context.hpp"
#include "ui/Surfaces.hpp"

namespace haylen::ui {

void Icon::readProperties(PropertyReader& reader) {
    reader.read("image", image);
    reader.read("size", size, 0.0F, 1024.0F);
    reader.read("color", color);
}

math::Vec2 Icon::measureContent(Context& context, float) {
    const float side = size > 0.0F ? size : context.getMetric(Theme::Metric::IconSize);
    return {side, side};
}

void Icon::render(Context& context, const math::Rect& bounds) {
    const float side = std::min(bounds.width, bounds.height);
    const math::Rect square{bounds.getCenter().x - side * 0.5F, bounds.getCenter().y - side * 0.5F, side, side};
    Surfaces::drawImage(context, context.getImage(image), square, color ? context.getColor(*color) : math::Color::white());
}

} // namespace haylen::ui
