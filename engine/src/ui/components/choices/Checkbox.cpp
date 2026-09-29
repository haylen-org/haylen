#include "ui/components/choices/Checkbox.hpp"

#include "haylen/ui/Context.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

void Checkbox::readProperties(PropertyReader& reader) {
    reader.read("text", text);
    reader.read("checked", checked);
}

math::Vec2 Checkbox::measureContent(Context& context, float) {
    return Widgets::measureChoice(context, context.getText(text));
}

void Checkbox::render(Context& context, const math::Rect& bounds) {
    if (Widgets::checkbox(context, bounds, checked, context.getText(text))) {
        context.emit(*this, "change", {{"checked", checked}});
    }
    if (takeFocusRequest()) {
        Widgets::focusItem(context);
    }
}

} // namespace haylen::ui
