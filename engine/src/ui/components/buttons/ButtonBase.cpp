#include "ui/components/buttons/ButtonBase.hpp"

#include "haylen/ui/Context.hpp"
#include "ui/Typography.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

void ButtonBase::readProperties(PropertyReader& reader) {
    reader.read("text", text);
    reader.read("icon", icon);
    if (const core::Json* value = reader.take("iconColor")) {
        iconColor = IconColor::fromJson(*value, reader.getQualifiedName("iconColor"));
    }
    reader.readChoice<Widgets::ButtonVariant>("variant", variant, Widgets::kButtonVariants);
    reader.readChoice<Widgets::IconPlacement>("iconPlacement", content.iconPlacement, Widgets::kIconPlacements);
    reader.readChoice<Alignment>("justify", content.justify, Typography::kAlignments);
    readMore(reader);
}

math::Vec2 ButtonBase::measureContent(Context& context, float availableWidth) {
    return Widgets::measureButton(context, context.getText(text), !icon.empty(), variant, content, availableWidth);
}

bool ButtonBase::drawButton(Context& context, const math::Rect& bounds, bool checked) {
    const float iconSize = context.getMetric(Theme::Metric::IconSize);
    const graphics::Texture iconTexture = icon.empty() ? graphics::Texture{} : context.getImage(icon, {iconSize, iconSize});
    const bool clicked = Widgets::button(context, bounds, context.getText(text), icon.empty() ? nullptr : &iconTexture, variant, checked, iconColor, content);
    if (takeFocusRequest()) {
        Widgets::focusItem(context);
    }
    return clicked;
}

bool ButtonBase::describe(Context& context, platform::AccessibilityElement& element) const {
    element.role = platform::AccessibilityElement::Role::Button;
    element.label.assign(context.getText(text));
    element.actions = platform::AccessibilityElement::Activate;
    return true;
}

} // namespace haylen::ui
