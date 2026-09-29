#include "ui/components/buttons/ImageButton.hpp"

#include <string>

#include "haylen/ui/Context.hpp"
#include "ui/Surfaces.hpp"
#include "ui/Typography.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

void ImageButton::readProperties(PropertyReader& reader) {
    reader.read("image", image);
    reader.read("hoverImage", hoverImage);
    reader.read("pressedImage", pressedImage);
    reader.read("text", text);
    reader.read("scale", scale, 0.0F, 64.0F);
    reader.read("tint", tint);
}

math::Vec2 ImageButton::measureContent(Context& context, float) {
    const graphics::Texture texture = context.getImage(image);
    return texture.isValid() ? texture.getSize() * scale : math::Vec2{context.getMetric(Theme::Metric::ControlHeight), context.getMetric(Theme::Metric::ControlHeight)};
}

void ImageButton::render(Context& context, const math::Rect& bounds) {
    const Widgets::Interaction state = Widgets::interact(context, bounds, context.getMetric(Theme::Metric::ControlRadius));
    if (takeFocusRequest()) {
        Widgets::focusItem(context);
    }
    const std::string& path = state.held && !pressedImage.empty() ? pressedImage : (state.hovered && !hoverImage.empty() ? hoverImage : image);
    const bool dimmed = state.hovered && hoverImage.empty() && !(state.held && !pressedImage.empty());
    const math::Color shade = dimmed ? math::Color{tint.r * 0.9F, tint.g * 0.9F, tint.b * 0.9F, tint.a} : tint;
    const float pressed = state.held && pressedImage.empty() ? 2.0F : 0.0F;
    const math::Rect area = bounds.translated({0.0F, pressed});
    Surfaces::drawImage(context, context.getImage(path), area, shade);
    if (const std::string shown = context.getText(text); !shown.empty()) {
        Typography::drawAligned(context, Theme::Font::Button, area, context.getColor(Theme::Color::OnAccent), shown, Alignment::Center);
    }
    if (state.clicked) {
        context.emit(*this, "click");
    }
}

} // namespace haylen::ui
