#include "platform/SystemLua.hpp"

#include <lua.hpp>

#include <string_view>

#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Promise.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/platform/System.hpp"

namespace haylen::platform {

int SystemLua::info(lua_State* L) {
    lua::JsonConverter::push(L, lua::Runtime::getEngine(L).getSystem().getInfo().toJson());
    return 1;
}

int SystemLua::theme(lua_State* L) {
    lua::Stack::push(L, System::themeName(lua::Runtime::getEngine(L).getSystem().getTheme()));
    return 1;
}

int SystemLua::battery(lua_State* L) {
    lua::JsonConverter::push(L, lua::Runtime::getEngine(L).getSystem().getBattery().toJson());
    return 1;
}

// Returns a promise that resolves on a later frame with whether an app took the url.
int SystemLua::openUrl(lua_State* L) {
    core::Engine& engine = lua::Runtime::getEngine(L);
    const auto url = lua::Stack::read<std::string_view>(L, 1);
    const lua::Promise promise(engine);
    engine.getSystem().openUrl(url, [promise](bool opened) { promise.resolve(opened); });
    promise.push(L);
    return 1;
}

int SystemLua::vibrate(lua_State* L) {
    lua::Runtime::getEngine(L).getSystem().vibrate(lua::Stack::read<float>(L, 1));
    return 0;
}

int SystemLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"info", &info}, {"theme", &theme}, {"battery", &battery}, {"openUrl", &lua::Binding::native<&openUrl>}, {"vibrate", &lua::Binding::native<&vibrate>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void SystemLua::install(lua_State* L) {
    lua::Binding::preload(L, "haylen.system", &open);
}

} // namespace haylen::platform
