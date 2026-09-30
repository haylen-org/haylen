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
}

math::Vec2 Image::measureContent(Context& context, float) {
    const graphics::Texture texture = context.getImage(image);
    return texture.isValid() ? texture.getSize() * scale : math::Vec2{};
}

void Image::render(Context& context, const math::Rect& bounds) {
    const graphics::Texture texture = context.getImage(image);
    if (!texture.isValid() || bounds.width <= 0.0F || bounds.height <= 0.0F) {
        return;
    }
    if (fit == Fit::Fill) {
        Surfaces::drawImage(context, texture, bounds, tint);
        return;
    }

    // The fit `contain` shows the whole picture inside the bounds, and `cover` fills them and crops what spills over.
    const math::Vec2 size = texture.getSize();
    const float factor = fit == Fit::Contain ? std::min(bounds.width / size.x, bounds.height / size.y) : std::max(bounds.width / size.x, bounds.height / size.y);
    const math::Vec2 shown = size * factor;
    if (fit == Fit::Contain) {
        Surfaces::drawImage(context, texture, {bounds.getCenter().x - shown.x * 0.5F, bounds.getCenter().y - shown.y * 0.5F, shown.x, shown.y}, tint);
        return;
    }
    const math::Vec2 visible{bounds.width / factor, bounds.height / factor};
    Surfaces::drawImage(context, texture, bounds, tint, {(size.x - visible.x) * 0.5F, (size.y - visible.y) * 0.5F, visible.x, visible.y});
}

} // namespace haylen::ui
