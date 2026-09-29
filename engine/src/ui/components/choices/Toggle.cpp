#include "ui/components/choices/Toggle.hpp"

#include "haylen/ui/Context.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

void Toggle::readProperties(PropertyReader& reader) {
    reader.read("text", text);
    reader.read("checked", checked);
}

math::Vec2 Toggle::measureContent(Context& context, float) {
    return Widgets::measureToggle(context, context.getText(text));
}

void Toggle::render(Context& context, const math::Rect& bounds) {
    if (Widgets::toggle(context, bounds, checked, context.getText(text))) {
        context.emit(*this, "change", {{"checked", checked}});
    }
    if (takeFocusRequest()) {
        Widgets::focusItem(context);
    }
}

} // namespace haylen::ui
