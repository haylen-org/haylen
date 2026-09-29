#include "ui/components/indicators/BusyIndicator.hpp"

#include <algorithm>

#include "haylen/ui/Context.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

void BusyIndicator::readProperties(PropertyReader& reader) {
    reader.read("size", size, 4.0F, 1024.0F);
    reader.read("color", color);
}

math::Vec2 BusyIndicator::measureContent(Context&, float) {
    return {size, size};
}

void BusyIndicator::render(Context& context, const math::Rect& bounds) {
    Widgets::spinner(context, bounds.getCenter(), std::min(bounds.width, bounds.height) * 0.45F, context.getColor(color.value_or(Theme::Color::Accent)));
}

} // namespace haylen::ui
