#include "platform/DisplayBinding.hpp"

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
#include "haylen/platform/System.hpp"
#include "platform/OutputBinding.hpp"
#include "platform/SystemAnswerBinding.hpp"

namespace haylen::lua::platform {

void DisplayBinding::pushDisplay(lua_State* L, const haylen::platform::Display& display) {
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
        const haylen::platform::Display::Mode& mode = display.modes[index];
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

int DisplayBinding::list(lua_State* L) {
    const std::span<const haylen::platform::Display> all = lua::Runtime::getEngine(L).getDisplays().getAll();
    lua_createtable(L, static_cast<int>(all.size()), 0);
    for (std::size_t index = 0; index < all.size(); ++index) {
        pushDisplay(L, all[index]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    return 1;
}

int DisplayBinding::current(lua_State* L) {
    pushDisplay(L, lua::Runtime::getEngine(L).getDisplays().getWindowDisplay());
    return 1;
}

int DisplayBinding::unlisted(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getDisplays().hasUnlisted());
    return 1;
}

// The list holds every display once the promise resolves, so the app reads them right after its await.
int DisplayBinding::requestEveryScreen(lua_State* L) {
    haylen::core::Engine& engine = lua::Runtime::getEngine(L);
    const lua::Promise promise(engine);
    promise.push(L);

    // clang-format off
    engine.getDisplays().requestEveryScreen(engine.getSystem().awaitAnswer([&engine, promise](const haylen::platform::SystemAnswer& answer) {
        if (answer.state == haylen::platform::SystemAnswer::State::Granted) {
            engine.getDisplays().refresh();
        }

        SystemAnswerBinding::settle(engine, promise, answer);
    }));
    // clang-format on

    return 1;
}

int DisplayBinding::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"list", &lua::Binding::native<&list>}, {"current", &lua::Binding::native<&current>}, {"unlisted", &lua::Binding::native<&unlisted>}, {"requestEveryScreen", &lua::Binding::native<&requestEveryScreen>}, {"open", &lua::Binding::native<&OutputBinding::open>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void DisplayBinding::install(lua_State* L) {
    OutputBinding::install(L);
    lua::Binding::preload(L, "haylen.display", &open);
}

} // namespace haylen::lua::platform
