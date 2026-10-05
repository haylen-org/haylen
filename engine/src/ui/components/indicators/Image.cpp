#include "ui/components/indicators/Image.hpp"

#include <algorithm>

#include "haylen/ui/Context.hpp"
#include "ui/Surfaces.hpp"

namespace haylen::ui {

void Image::readProperties(PropertyReader& reader) {
    reader.read("image", image);
    reader.readChoice<Fit>("fit", fit, kFits);
    reader.read("scale", scale, 0.0F, 64.0F);
    reader.read("tint", tint);
    reader.read("radius", radius, 0.0F, 4096.0F);
}

math::Vec2 Image::measureContent(Context& context, float) {
    return context.getImageSize(image) * scale;
}

void Image::render(Context& context, const math::Rect& bounds) {
    const math::Vec2 natural = context.getImageSize(image);
    if (natural.x <= 0.0F || natural.y <= 0.0F || bounds.width <= 0.0F || bounds.height <= 0.0F) {
        return;
    }
    if (fit == Fit::Fill) {
        Surfaces::drawImage(context, context.getImage(image, bounds.getSize()), bounds, tint, {}, radius);
        return;
    }

    // The fit `contain` shows the whole picture inside the bounds, and `cover` fills them and crops what spills over.
    const float factor = fit == Fit::Contain ? std::min(bounds.width / natural.x, bounds.height / natural.y) : std::max(bounds.width / natural.x, bounds.height / natural.y);
    const math::Vec2 shown = natural * factor;
    const graphics::Texture texture = context.getImage(image, shown);
    if (fit == Fit::Contain) {
        Surfaces::drawImage(context, texture, {bounds.getCenter().x - shown.x * 0.5F, bounds.getCenter().y - shown.y * 0.5F, shown.x, shown.y}, tint, {}, radius);
        return;
    }
    if (!texture.isValid()) {
        return;
    }
    const math::Vec2 pixels{texture.getSize().x / natural.x, texture.getSize().y / natural.y};
    const math::Vec2 visible{bounds.width / factor, bounds.height / factor};
    Surfaces::drawImage(context, texture, bounds, tint, {(natural.x - visible.x) * 0.5F * pixels.x, (natural.y - visible.y) * 0.5F * pixels.y, visible.x * pixels.x, visible.y * pixels.y}, radius);
}

} // namespace haylen::ui
