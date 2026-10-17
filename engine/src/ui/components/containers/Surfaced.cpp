#include "ui/components/containers/Surfaced.hpp"

#include <optional>

#include "haylen/math/Insets.hpp"
#include "haylen/ui/Context.hpp"
#include "ui/Surfaces.hpp"

namespace haylen::ui {

math::Insets Surfaced::getPadding(Context& context) const {
    const math::Insets own = Linear::getPadding(context);
    const math::Insets image = Surfaces::getPadding(context, surface);
    const float panel = own == math::Insets{} ? context.getMetric(Theme::Metric::PanelPadding) : 0.0F;
    return {own.left + image.left + panel, own.top + image.top + panel, own.right + image.right + panel, own.bottom + image.bottom + panel};
}

void Surfaced::paint(Context& context, const math::Rect& bounds) {
    context.blockPointer(bounds);
    const std::optional<math::Color> border = bordered ? std::optional<math::Color>(context.getColor(Theme::Color::Border)) : std::nullopt;
    Surfaces::draw(context, surface, bounds, context.getColor(fill), border, context.getMetric(Theme::Metric::PanelRadius));
}

// The nodes inside round their highlights concentric with the corners of the surface, which a surface that a theme paints with an image leaves to the image.
void Surfaced::render(Context& context, const math::Rect& bounds) {
    context.pushEnclosure(bounds, context.getSurface(surface) != nullptr ? 0.0F : context.getMetric(Theme::Metric::PanelRadius));
    Linear::render(context, bounds);
    context.popEnclosure();
}

} // namespace haylen::ui
