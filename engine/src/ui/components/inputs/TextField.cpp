#include "ui/components/inputs/TextField.hpp"

#include "haylen/ui/Context.hpp"

namespace haylen::ui {

void TextField::readMore(PropertyReader& reader) {
    reader.readChoice<platform::TextInput::Keyboard>("keyboard", keyboard, kKeyboards);
}

void TextField::render(Context& context, const math::Rect& bounds) {
    drawEntry(context, bounds, keyboard);
}

} // namespace haylen::ui
