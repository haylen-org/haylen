#include "haylen/input/VirtualInput.hpp"

namespace haylen::input {

void VirtualInput::setButton(std::string_view name, bool down) {
    buttons.insert_or_assign(std::string(name), down);
}

void VirtualInput::setStick(std::string_view name, math::Vec2 value) {
    sticks.insert_or_assign(std::string(name), value.clampedLength(1.0F));
}

void VirtualInput::clear() noexcept {
    buttons.clear();
    sticks.clear();
}

bool VirtualInput::isButtonDown(std::string_view name) const noexcept {
    const auto found = buttons.find(name);
    return found != buttons.end() && found->second;
}

math::Vec2 VirtualInput::getStick(std::string_view name) const noexcept {
    const auto found = sticks.find(name);
    return found == sticks.end() ? math::Vec2{} : found->second;
}

} // namespace haylen::input
