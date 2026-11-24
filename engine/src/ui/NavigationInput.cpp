#include "haylen/ui/NavigationInput.hpp"

#include <algorithm>
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
                        {{"name", "uiFocus"}, {"type", "button"}, {"bindings", {"button:back"}}},
                        {{"name", "uiPagePrevious"}, {"type", "button"}, {"bindings", {"key:pageUp", "button:leftShoulder"}}},
                        {{"name", "uiPageNext"}, {"type", "button"}, {"bindings", {"key:pageDown", "button:rightShoulder"}}},
                        {{"name", "uiFirst"}, {"type", "button"}, {"bindings", {"key:home"}}},
                        {{"name", "uiLast"}, {"type", "button"}, {"bindings", {"key:end"}}},
                    }}});
    resolved = defaults;
}

void NavigationInput::update(const input::ActionMap& actions, const input::Input& devices, const input::VirtualInput& virtualInput, bool blocked, bool playing) {
    wasDown = down;

    for (const std::string_view name : kNames) {
        const input::ActionMap::Action* remapped = actions.findAction(name);
        const input::ActionMap::Action& wanted = remapped != nullptr ? *remapped : *defaults.findAction(name);
        if (*resolved.findAction(name) != wanted) {
            resolved.define(wanted);
        }
    }

    resolved.update(devices, virtualInput, blocked);

    // A stick that leans past the threshold along both axes moves the focus along the axis it leans the most, the way a directional pad presses one direction, while two directions of the same strength, such as two buttons of a pad, both stay down.
    const float horizontal = std::max(resolved.getValue(kNames[static_cast<std::size_t>(Action::Left)]), resolved.getValue(kNames[static_cast<std::size_t>(Action::Right)]));
    const float vertical = std::max(resolved.getValue(kNames[static_cast<std::size_t>(Action::Up)]), resolved.getValue(kNames[static_cast<std::size_t>(Action::Down)]));
    for (std::size_t index = 0; index < kActionCount; ++index) {
        const auto action = static_cast<Action>(index);
        const bool sideways = action == Action::Left || action == Action::Right;
        const bool upright = action == Action::Up || action == Action::Down;
        const bool weaker = (sideways && vertical > horizontal) || (upright && horizontal > vertical);
        const bool pressed = resolved.isDown(kNames[index]) && !weaker;
        const bool game = action != Action::Cancel && action != Action::Focus;
        held[index] = pressed && (held[index] || (playing && game && !wasDown[index]));
        down[index] = pressed && !held[index];
    }
}

const std::vector<input::ActionMap::Binding>& NavigationInput::getBindings(Action action) const {
    return resolved.findAction(kNames[static_cast<std::size_t>(action)])->bindings;
}

} // namespace haylen::ui
