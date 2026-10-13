#include "platform/AccessibilityLua.hpp"

#include <lua.hpp>

#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/platform/Accessibility.hpp"

namespace haylen::platform {

int AccessibilityLua::settings(lua_State* L) {
    lua::JsonConverter::push(L, lua::Runtime::getEngine(L).getAccessibility().getSettings().toJson());
    return 1;
}

int AccessibilityLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"settings", &settings},
        {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void AccessibilityLua::install(lua_State* L) {
    lua::Binding::preload(L, "haylen.accessibility", &open);
}

} // namespace haylen::platform
