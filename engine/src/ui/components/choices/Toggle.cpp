#include "ui/components/choices/Toggle.hpp"

#include "haylen/ui/Context.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

void Toggle::readProperties(PropertyReader& reader) {
    reader.read("text", text);
    reader.read("checked", checked);
}

math::Vec2 Toggle::measureContent(Context& context, float availableWidth) {
    return Widgets::measureToggle(context, context.getText(text), availableWidth);
}

void Toggle::render(Context& context, const math::Rect& bounds) {
    if (Widgets::toggle(context, bounds, checked, context.getText(text))) {
        context.emit(*this, "change", {{"checked", checked}});
    }
    if (takeFocusRequest()) {
        Widgets::focusItem(context);
    }
}

bool Toggle::describe(Context& context, platform::AccessibilityElement& element) const {
    element.role = platform::AccessibilityElement::Role::Switch;
    element.label.assign(context.getText(text));
    element.states = checked ? platform::AccessibilityElement::Checked : 0;
    element.actions = platform::AccessibilityElement::Activate;
    return true;
}

} // namespace haylen::ui
