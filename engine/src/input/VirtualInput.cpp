#include "haylen/input/VirtualInput.hpp"

namespace haylen::input {

// A control that stays held writes its name every frame, so a name the map knows changes in place, and only a new name builds a string.
template <typename Value> void VirtualInput::assign(std::unordered_map<std::string, Value, NameHash, std::equal_to<>>& controls, std::string_view name, Value value) {
    const auto found = controls.find(name);
    if (found != controls.end()) {
        found->second = value;
        return;
    }

    controls.emplace(name, value);
}

void VirtualInput::setButton(std::string_view name, bool down) {
    assign(buttons, name, down);
}

void VirtualInput::setStick(std::string_view name, math::Vec2 value) {
    assign(sticks, name, value.clampedLength(1.0F));
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
