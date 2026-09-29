#include "ui/components/buttons/ButtonBase.hpp"

#include "haylen/ui/Context.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

void ButtonBase::readProperties(PropertyReader& reader) {
    reader.read("text", text);
    reader.read("icon", icon);
    reader.readChoice<Widgets::ButtonVariant>("variant", variant, Widgets::kButtonVariants);
    readMore(reader);
}

math::Vec2 ButtonBase::measureContent(Context& context, float) {
    return Widgets::measureButton(context, context.getText(text), !icon.empty(), variant);
}

bool ButtonBase::drawButton(Context& context, const math::Rect& bounds, bool checked) {
    const graphics::Texture iconTexture = icon.empty() ? graphics::Texture{} : context.getImage(icon);
    const bool clicked = Widgets::button(context, bounds, context.getText(text), icon.empty() ? nullptr : &iconTexture, variant, checked);
    if (takeFocusRequest()) {
        Widgets::focusItem(context);
    }
    return clicked;
}

} // namespace haylen::ui
