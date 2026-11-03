#include "platform/DisplayLua.hpp"

#include <lua.hpp>

#include <cstddef>
#include <span>

#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/Promise.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/platform/Displays.hpp"
#include "platform/OutputLua.hpp"

namespace haylen::platform {

void DisplayLua::pushDisplay(lua_State* L, const Display& display) {
    lua_createtable(L, 0, 8);
    lua::Stack::push(L, display.id);
    lua_setfield(L, -2, "id");
    lua::Stack::push(L, display.name);
    lua_setfield(L, -2, "name");
    lua::Stack::push(L, display.bounds);
    lua_setfield(L, -2, "bounds");
    lua::Stack::push(L, display.workArea);
    lua_setfield(L, -2, "workArea");
    lua::Stack::push(L, display.scale);
    lua_setfield(L, -2, "scale");
    lua::Stack::push(L, display.refreshRate);
    lua_setfield(L, -2, "refreshRate");
    lua::Stack::push(L, display.primary);
    lua_setfield(L, -2, "primary");

    lua_createtable(L, static_cast<int>(display.modes.size()), 0);
    for (std::size_t index = 0; index < display.modes.size(); ++index) {
        const Display::Mode& mode = display.modes[index];
        lua_createtable(L, 0, 4);
        lua::Stack::push(L, mode.width);
        lua_setfield(L, -2, "width");
        lua::Stack::push(L, mode.height);
        lua_setfield(L, -2, "height");
        lua::Stack::push(L, mode.refreshRate);
        lua_setfield(L, -2, "refreshRate");
        lua::Stack::push(L, mode.scale);
        lua_setfield(L, -2, "scale");
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    lua_setfield(L, -2, "modes");
}

int DisplayLua::list(lua_State* L) {
    const std::span<const Display> all = lua::Runtime::getEngine(L).getDisplays().getAll();
    lua_createtable(L, static_cast<int>(all.size()), 0);
    for (std::size_t index = 0; index < all.size(); ++index) {
        pushDisplay(L, all[index]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    return 1;
}

int DisplayLua::current(lua_State* L) {
    pushDisplay(L, lua::Runtime::getEngine(L).getDisplays().getWindowDisplay());
    return 1;
}

// Resolves with `true` once every display is listed, and rejects with the code `denied` when the person or the browser refused, or `unsupported` where the browser cannot list its screens. The app asked, so a rejection that nothing awaits never stops it.
int DisplayLua::request(lua_State* L) {
    core::Engine& engine = lua::Runtime::getEngine(L);
    const lua::Promise promise(engine);
    // clang-format off
    engine.getDisplays().request([promise, &engine](Window::DisplayAccess access) {
        if (access == Window::DisplayAccess::Granted) {
            promise.resolve(true);
            return;
        }
        lua_State* main = engine.getLuaState();
        const bool denied = access == Window::DisplayAccess::Denied;
        lua_createtable(main, 0, 2);
        lua::Stack::push(main, denied ? "The person or the browser did not let the app see every display." : "The browser cannot list the screens of the device, since it has no Window Management API.");
        lua_setfield(main, -2, "error");
        lua::Stack::push(main, denied ? "denied" : "unsupported");
        lua_setfield(main, -2, "code");
        promise.rejectHandledFailure(main);
    });
    // clang-format on
    promise.push(L);
    return 1;
}

int DisplayLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"list", &lua::Binding::native<&list>}, {"current", &lua::Binding::native<&current>}, {"request", &lua::Binding::native<&request>}, {"open", &lua::Binding::native<&OutputLua::open>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void DisplayLua::install(lua_State* L) {
    OutputLua::install(L);
    lua::Binding::preload(L, "haylen.display", &open);
}

} // namespace haylen::platform
