#include "ui/components/buttons/Button.hpp"

#include "haylen/ui/Context.hpp"

namespace haylen::ui {

void Button::readMore(PropertyReader& reader) {
    reader.read("checked", checked);
}

void Button::render(Context& context, const math::Rect& bounds) {
    if (drawButton(context, bounds, checked)) {
        context.emit(*this, "click");
    }
}

bool Button::describe(Context& context, platform::AccessibilityElement& element) const {
    (void)ButtonBase::describe(context, element);
    if (checked) {
        element.states |= platform::AccessibilityElement::Selected;
    }
    return true;
}

} // namespace haylen::ui
