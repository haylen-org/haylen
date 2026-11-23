#include "platform/AccessibilityBinding.hpp"

#include <lua.hpp>

#include <optional>
#include <string>
#include <string_view>

#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/platform/Accessibility.hpp"

namespace haylen::lua::platform {

int AccessibilityBinding::settings(lua_State* L) {
    lua::JsonConverter::push(L, lua::Runtime::getEngine(L).getAccessibility().getSettings().toJson());
    return 1;
}

// Says a text with `announce(text, {interrupt})`, after what the screen reader says now, or at once with `interrupt`.
int AccessibilityBinding::announce(lua_State* L) {
    const auto text = lua::Stack::read<std::string_view>(L, 1);
    bool interrupt = false;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kAnnounceOptions});
        lua::Table::readField(L, 2, "interrupt", interrupt);
    }

    lua::Runtime::getEngine(L).getAccessibility().announce(text, interrupt);
    return 0;
}

// An element is a table with its id, the id of its parent, its role, label, value and hint, its bounds in design units, a boolean for each state it is in and the list of its actions.
void AccessibilityBinding::pushElement(lua_State* L, const haylen::platform::AccessibilityElement& element) {
    const haylen::graphics::Viewport& viewport = lua::Runtime::getEngine(L).getViewport();
    lua_createtable(L, 0, 12);
    lua::Stack::push(L, element.id);
    lua_setfield(L, -2, "id");
    lua::Stack::push(L, element.parent);
    lua_setfield(L, -2, "parent");
    lua::Stack::push(L, haylen::platform::AccessibilityElement::roleName(element.role));
    lua_setfield(L, -2, "role");
    for (const auto& [name, text] : {std::pair{"label", &element.label}, std::pair{"value", &element.value}, std::pair{"hint", &element.hint}}) {
        lua::Stack::push(L, *text);
        lua_setfield(L, -2, name);
    }

    const haylen::math::Rect bounds = haylen::math::Rect::fromMinMax(viewport.toDesign(element.bounds.getMin()), viewport.toDesign(element.bounds.getMax()));
    lua::Stack::push(L, bounds);
    lua_setfield(L, -2, "bounds");
    for (const auto& [name, state] : haylen::platform::AccessibilityElement::kStateNames) {
        lua::Stack::push(L, element.has(state));
        lua_setfield(L, -2, std::string(name).c_str());
    }

    lua_createtable(L, 4, 0);
    lua_Integer count = 0;
    for (const auto& [name, action] : haylen::platform::AccessibilityElement::kActionNames) {
        if (element.can(action)) {
            lua::Stack::push(L, name);
            lua_rawseti(L, -2, ++count);
        }
    }

    lua_setfield(L, -2, "actions");
}

// Returns the elements of the last frame in the order screen readers move through them, which the UI builds while a screen reader runs or `setTreeEnabled` turned the tree on.
int AccessibilityBinding::elements(lua_State* L) {
    const std::span<const haylen::platform::AccessibilityElement> tree = lua::Runtime::getEngine(L).getAccessibility().getElements();
    lua_createtable(L, static_cast<int>(tree.size()), 0);
    for (std::size_t index = 0; index < tree.size(); ++index) {
        pushElement(L, tree[index]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    return 1;
}

int AccessibilityBinding::focused(lua_State* L) {
    const std::uint64_t id = lua::Runtime::getEngine(L).getAccessibility().getFocused();
    if (id == 0) {
        lua_pushnil(L);
    } else {
        lua::Stack::push(L, id);
    }

    return 1;
}

// Asks an element for an action with `perform(id, action)`, the way a screen reader does, which the UI performs at its next frame.
int AccessibilityBinding::perform(lua_State* L) {
    const auto id = lua::Stack::read<std::uint64_t>(L, 1);
    const auto name = lua::Stack::read<std::string_view>(L, 2);
    const std::optional<haylen::platform::AccessibilityElement::Action> action = haylen::platform::AccessibilityElement::actionFromName(name);
    if (!action) {
        return luaL_error(L, "The accessibility action \"%s\" is unknown. It is \"focus\", \"activate\", \"increment\" or \"decrement\".", std::string(name).c_str());
    }

    lua::Runtime::getEngine(L).getAccessibility().perform({.element = id, .action = *action});
    return 0;
}

int AccessibilityBinding::setTreeEnabled(lua_State* L) {
    lua::Runtime::getEngine(L).getAccessibility().setTreeEnabled(lua::Stack::read<bool>(L, 1));
    return 0;
}

int AccessibilityBinding::treeEnabled(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getAccessibility().isTreeEnabled());
    return 1;
}

int AccessibilityBinding::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"settings", &settings}, {"announce", &lua::Binding::native<&announce>}, {"elements", &elements}, {"focused", &focused}, {"perform", &lua::Binding::native<&perform>}, {"setTreeEnabled", &lua::Binding::native<&setTreeEnabled>}, {"treeEnabled", &treeEnabled}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void AccessibilityBinding::install(lua_State* L) {
    lua::Binding::preload(L, "haylen.accessibility", &open);
}

} // namespace haylen::lua::platform
