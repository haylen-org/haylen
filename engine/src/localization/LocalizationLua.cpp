#include "localization/LocalizationLua.hpp"

#include <lua.hpp>

#include <string>
#include <string_view>

#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/plugins/LocalizationPlugin.hpp"

namespace haylen::localization {

Catalog& LocalizationLua::getCatalog(lua_State* L) {
    return lua::Runtime::getEngine(L).getPlugin<plugins::LocalizationPlugin>().getCatalog();
}

int LocalizationLua::add(lua_State* L) {
    getCatalog(L).add(lua::Stack::read<std::string>(L, 1), lua::JsonConverter::read(L, 2));
    return 0;
}

int LocalizationLua::loadFolder(lua_State* L) {
    core::Engine& owner = lua::Runtime::getEngine(L);
    lua::Stack::push(L, owner.getPlugin<plugins::LocalizationPlugin>().loadFolder(owner.getPackage(), lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int LocalizationLua::setLanguage(lua_State* L) {
    getCatalog(L).setLanguage(lua::Stack::read<std::string_view>(L, 1));
    return 0;
}

int LocalizationLua::language(lua_State* L) {
    lua::Stack::push(L, getCatalog(L).getLanguage());
    return 1;
}

int LocalizationLua::setFallback(lua_State* L) {
    getCatalog(L).setFallback(lua::Stack::read<std::string_view>(L, 1));
    return 0;
}

int LocalizationLua::fallback(lua_State* L) {
    lua::Stack::push(L, getCatalog(L).getFallback());
    return 1;
}

int LocalizationLua::languages(lua_State* L) {
    lua::Stack::push(L, getCatalog(L).getLanguages());
    return 1;
}

int LocalizationLua::has(lua_State* L) {
    lua::Stack::push(L, getCatalog(L).has(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

// Translates with text(key[, arguments]), where arguments fill {name} placeholders and count picks the plural form.
int LocalizationLua::text(lua_State* L) {
    const core::Json arguments = lua_isnoneornil(L, 2) ? core::Json::object() : lua::JsonConverter::read(L, 2);
    lua::Stack::push(L, getCatalog(L).getText(lua::Stack::read<std::string_view>(L, 1), arguments));
    return 1;
}

// Returns with direction([language]) the direction the current or the named language declares, ltr or rtl.
int LocalizationLua::direction(lua_State* L) {
    const Catalog& catalog = getCatalog(L);
    const std::string language = lua_isnoneornil(L, 1) ? catalog.getLanguage() : lua::Stack::read<std::string>(L, 1);
    lua::Stack::push(L, std::string_view(catalog.getDirection(language) == haylen::text::Direction::RightToLeft ? "rtl" : "ltr"));
    return 1;
}

int LocalizationLua::findBestMatch(lua_State* L) {
    lua::Stack::push(L, getCatalog(L).findBestMatch(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int LocalizationLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"add", &lua::Binding::native<&add>}, {"loadFolder", &lua::Binding::native<&loadFolder>}, {"setLanguage", &lua::Binding::native<&setLanguage>}, {"language", &lua::Binding::native<&language>}, {"setFallback", &lua::Binding::native<&setFallback>}, {"fallback", &lua::Binding::native<&fallback>}, {"languages", &lua::Binding::native<&languages>}, {"has", &lua::Binding::native<&has>}, {"text", &lua::Binding::native<&text>}, {"findBestMatch", &lua::Binding::native<&findBestMatch>}, {"direction", &lua::Binding::native<&direction>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void LocalizationLua::install(lua_State* L) {
    lua::Binding::preload(L, "haylen.localization", &open);
}

} // namespace haylen::localization
