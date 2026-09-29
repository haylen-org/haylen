#include "haylen/ui/NavigationInput.hpp"

#include <string>

#include "haylen/core/Json.hpp"

namespace haylen::ui {

NavigationInput::NavigationInput() {
    defaults.load({{"actions",
                    {
                        {{"name", "ui_accept"}, {"type", "button"}, {"bindings", {"key:enter", "key:keypad_enter", "key:space", "button:south"}}},
                        {{"name", "ui_cancel"}, {"type", "button"}, {"bindings", {"key:escape", "button:east"}}},
                        {{"name", "ui_left"}, {"type", "button"}, {"bindings", {"key:left", "button:dpad_left", "axis:left_x-"}}},
                        {{"name", "ui_right"}, {"type", "button"}, {"bindings", {"key:right", "button:dpad_right", "axis:left_x+"}}},
                        {{"name", "ui_up"}, {"type", "button"}, {"bindings", {"key:up", "button:dpad_up", "axis:left_y-"}}},
                        {{"name", "ui_down"}, {"type", "button"}, {"bindings", {"key:down", "button:dpad_down", "axis:left_y+"}}},
                        {{"name", "ui_menu"}, {"type", "button"}, {"bindings", {"key:menu", "button:north"}}},
                    }}});
    resolved = defaults;
}

void NavigationInput::update(const input::ActionMap& actions, const input::Input& devices, const input::VirtualInput& virtualInput, bool blocked) {
    wasDown = down;
    for (const std::string_view name : kNames) {
        const input::ActionMap::Action* remapped = actions.findAction(name);
        const input::ActionMap::Action& wanted = remapped != nullptr ? *remapped : *defaults.findAction(name);
        if (*resolved.findAction(name) != wanted) {
            resolved.define(wanted);
        }
    }
    resolved.update(blocked ? idleInput : devices, blocked ? idleVirtualInput : virtualInput);
    for (std::size_t index = 0; index < kActionCount; ++index) {
        down[index] = resolved.isDown(kNames[index]);
    }
}

const std::vector<input::ActionMap::Binding>& NavigationInput::getBindings(Action action) const {
    return resolved.findAction(kNames[static_cast<std::size_t>(action)])->bindings;
}

} // namespace haylen::ui
