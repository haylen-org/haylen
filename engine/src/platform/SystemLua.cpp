#include "platform/SystemLua.hpp"

#include <lua.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "haylen/core/Engine.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Promise.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/platform/System.hpp"
#include "haylen/platform/SystemAnswer.hpp"
#include "platform/SystemAnswerLua.hpp"

namespace haylen::platform {

int SystemLua::info(lua_State* L) {
    lua::JsonConverter::push(L, lua::Runtime::getEngine(L).getSystem().getInfo().toJson());
    return 1;
}

int SystemLua::theme(lua_State* L) {
    lua::Stack::push(L, System::themeName(lua::Runtime::getEngine(L).getSystem().getTheme()));
    return 1;
}

int SystemLua::languages(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getSystem().getLanguages());
    return 1;
}

int SystemLua::battery(lua_State* L) {
    lua::JsonConverter::push(L, lua::Runtime::getEngine(L).getSystem().getBattery().toJson());
    return 1;
}

// Returns whether the power saving mode of the system is on, or nil where the platform does not tell.
int SystemLua::powerSaving(lua_State* L) {
    const std::optional<bool> enabled = lua::Runtime::getEngine(L).getSystem().getPowerSaving();
    if (!enabled) {
        lua_pushnil(L);
        return 1;
    }

    lua::Stack::push(L, *enabled);
    return 1;
}

// Returns the IANA name of the time zone, or nil where the platform does not tell.
int SystemLua::timeZone(lua_State* L) {
    const std::string& zone = lua::Runtime::getEngine(L).getSystem().getTimeZone();
    if (zone.empty()) {
        lua_pushnil(L);
        return 1;
    }

    lua::Stack::push(L, zone);
    return 1;
}

// Returns a promise that resolves on a later frame with the text on the clipboard.
int SystemLua::clipboard(lua_State* L) {
    core::Engine& engine = lua::Runtime::getEngine(L);
    const lua::Promise promise(engine);
    engine.getSystem().readClipboard([&engine, promise](SystemAnswer answer, std::string text) { SystemAnswerLua::settle(engine, promise, answer, std::move(text)); });
    promise.push(L);
    return 1;
}

// Returns a promise that resolves on a later frame once the text reached the clipboard.
int SystemLua::setClipboard(lua_State* L) {
    core::Engine& engine = lua::Runtime::getEngine(L);
    const auto text = lua::Stack::read<std::string_view>(L, 1);
    const lua::Promise promise(engine);
    engine.getSystem().setClipboard(text, [&engine, promise](SystemAnswer answer) { SystemAnswerLua::settle(engine, promise, answer); });
    promise.push(L);
    return 1;
}

int SystemLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"info", &info}, {"theme", &theme}, {"battery", &battery}, {"powerSaving", &powerSaving}, {"timeZone", &timeZone}, {"languages", &languages}, {"clipboard", &lua::Binding::native<&clipboard>}, {"setClipboard", &lua::Binding::native<&setClipboard>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void SystemLua::install(lua_State* L) {
    lua::Binding::preload(L, "haylen.system", &open);
}

} // namespace haylen::platform
