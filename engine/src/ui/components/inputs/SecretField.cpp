#include "ui/components/inputs/SecretField.hpp"

#include "haylen/ui/Context.hpp"

namespace haylen::ui {

void SecretField::render(Context& context, const math::Rect& bounds) {
    drawEntry(context, bounds, platform::TextInput::Keyboard::Password);
}

} // namespace haylen::ui
