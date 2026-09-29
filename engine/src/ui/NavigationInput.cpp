#include "haylen/ui/NavigationInput.hpp"

#include <string>

#include "haylen/core/Json.hpp"

namespace haylen::ui {

NavigationInput::NavigationInput() {
    defaults.load({{"actions",
                    {
                        {{"name", "uiAccept"}, {"type", "button"}, {"bindings", {"key:enter", "key:keypadEnter", "key:space", "button:south"}}},
                        {{"name", "uiCancel"}, {"type", "button"}, {"bindings", {"key:escape", "button:east"}}},
                        {{"name", "uiLeft"}, {"type", "button"}, {"bindings", {"key:left", "button:dpadLeft", "axis:leftX-"}}},
                        {{"name", "uiRight"}, {"type", "button"}, {"bindings", {"key:right", "button:dpadRight", "axis:leftX+"}}},
                        {{"name", "uiUp"}, {"type", "button"}, {"bindings", {"key:up", "button:dpadUp", "axis:leftY-"}}},
                        {{"name", "uiDown"}, {"type", "button"}, {"bindings", {"key:down", "button:dpadDown", "axis:leftY+"}}},
                        {{"name", "uiMenu"}, {"type", "button"}, {"bindings", {"key:menu", "button:north"}}},
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
    resolved.update(devices, virtualInput, blocked);
    for (std::size_t index = 0; index < kActionCount; ++index) {
        down[index] = resolved.isDown(kNames[index]);
    }
}

const std::vector<input::ActionMap::Binding>& NavigationInput::getBindings(Action action) const {
    return resolved.findAction(kNames[static_cast<std::size_t>(action)])->bindings;
}

} // namespace haylen::ui
