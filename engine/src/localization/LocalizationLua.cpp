#include "localization/LocalizationLua.hpp"

#include <lua.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/localization/DateFormat.hpp"
#include "haylen/localization/ListFormat.hpp"
#include "haylen/localization/NumberFormat.hpp"
#include "haylen/localization/PluralRules.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/plugins/LocalizationPlugin.hpp"
#include "haylen/text/Style.hpp"

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

int LocalizationLua::setPseudo(lua_State* L) {
    getCatalog(L).setPseudo(lua::Stack::read<bool>(L, 1));
    return 0;
}

int LocalizationLua::pseudo(lua_State* L) {
    lua::Stack::push(L, getCatalog(L).isPseudo());
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

// Translates with `text(key[, arguments])`, where `arguments` fill `{name}` placeholders and `count` picks the plural form.
int LocalizationLua::text(lua_State* L) {
    const core::Json arguments = lua_isnoneornil(L, 2) ? core::Json::object() : lua::JsonConverter::read(L, 2);
    lua::Stack::push(L, getCatalog(L).getText(lua::Stack::read<std::string_view>(L, 1), arguments));
    return 1;
}

// Returns with `direction([language])` the direction the current or the named language declares, `leftToRight` or `rightToLeft`.
int LocalizationLua::direction(lua_State* L) {
    const Catalog& catalog = getCatalog(L);
    const std::string language = lua_isnoneornil(L, 1) ? catalog.getLanguage() : lua::Stack::read<std::string>(L, 1);
    lua::Stack::push(L, haylen::text::Style::directionName(catalog.getDirection(language)));
    return 1;
}

int LocalizationLua::findBestMatch(lua_State* L) {
    lua::Stack::push(L, getCatalog(L).findBestMatch(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

std::string LocalizationLua::readLanguage(lua_State* L, int index) {
    std::string language = getCatalog(L).getLanguage();
    if (lua_istable(L, index)) {
        lua::Table::readField(L, index, "language", language);
    }
    return language;
}

// Writes a number with `formatNumber(value[, {style, minimumFractionDigits, maximumFractionDigits, currency, grouping, language}])`.
int LocalizationLua::formatNumber(lua_State* L) {
    const double value = luaL_checknumber(L, 1);
    NumberFormat::Options options;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kNumberOptions});
        std::string style;
        lua::Table::readField(L, 2, "style", style);
        if (!style.empty()) {
            const std::optional<NumberFormat::Style> found = NumberFormat::styleFromName(style);
            if (!found) {
                return luaL_error(L, "The style of a number must be \"decimal\", \"percent\", \"currency\" or \"compact\".");
            }
            options.style = *found;
        }
        lua::Table::readField(L, 2, "minimumFractionDigits", options.minimumFractionDigits);
        lua::Table::readField(L, 2, "maximumFractionDigits", options.maximumFractionDigits);
        lua::Table::readField(L, 2, "currency", options.currency);
        lua::Table::readField(L, 2, "grouping", options.grouping);
    }
    lua::Stack::push(L, NumberFormat::format(readLanguage(L, 2), value, options));
    return 1;
}

// Writes a moment in Unix seconds with `formatDate(seconds[, {date = 'medium', time = false, timeZoneOffset, language}])`, where `date = false` leaves the date out.
int LocalizationLua::formatDate(lua_State* L) {
    const auto seconds = static_cast<std::int64_t>(luaL_checknumber(L, 1));
    DateFormat::Options options;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kDateOptions});
        if (lua_getfield(L, 2, "date") == LUA_TBOOLEAN && lua_toboolean(L, -1) == 0) {
            options.date.reset();
        } else if (!lua_isnil(L, -1)) {
            const std::optional<DateFormat::Style> found = lua_type(L, -1) == LUA_TSTRING ? DateFormat::styleFromName(lua_tostring(L, -1)) : std::nullopt;
            if (!found) {
                return luaL_error(L, "The date of a date format must be \"short\", \"medium\", \"long\", \"full\" or false.");
            }
            options.date = *found;
        }
        lua_pop(L, 1);
        lua::Table::readField(L, 2, "time", options.time);
        lua::Table::readField(L, 2, "timeZoneOffset", options.timeZoneOffset);
    }
    lua::Stack::push(L, DateFormat::format(readLanguage(L, 2), seconds, options));
    return 1;
}

// Joins strings with `formatList(items[, {type = 'and', language}])`.
int LocalizationLua::formatList(lua_State* L) {
    const std::vector<std::string> items = lua::Stack::read<std::vector<std::string>>(L, 1);
    ListFormat::Type type = ListFormat::Type::And;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kListOptions});
        std::string name;
        lua::Table::readField(L, 2, "type", name);
        if (!name.empty()) {
            const std::optional<ListFormat::Type> found = ListFormat::typeFromName(name);
            if (!found) {
                return luaL_error(L, "The type of a list must be \"and\" or \"or\".");
            }
            type = *found;
        }
    }
    lua::Stack::push(L, ListFormat::format(readLanguage(L, 2), items, type));
    return 1;
}

// Returns the plural category of a count with `pluralCategory(count[, {ordinal = false, language}])`.
int LocalizationLua::pluralCategory(lua_State* L) {
    const double count = luaL_checknumber(L, 1);
    bool ordinal = false;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kPluralOptions});
        lua::Table::readField(L, 2, "ordinal", ordinal);
    }
    const std::string language = readLanguage(L, 2);
    lua::Stack::push(L, PluralRules::categoryName(ordinal ? PluralRules::selectOrdinal(language, count) : PluralRules::select(language, count)));
    return 1;
}

// Returns `i18n/<language>/<path>` when the package holds that asset for the current language, then for its base language, and the path itself otherwise.
int LocalizationLua::assetPath(lua_State* L) {
    const std::string_view path = lua::Stack::read<std::string_view>(L, 1);
    const std::string language = getCatalog(L).getLanguage();
    const assets::Manager& assets = lua::Runtime::getEngine(L).getAssets();
    for (const std::string_view tag : {std::string_view(language), std::string_view(language).substr(0, language.find_first_of("-_"))}) {
        std::string localized = "i18n/" + std::string(tag) + "/" + std::string(path);
        if (!tag.empty() && assets.exists(localized)) {
            lua::Stack::push(L, localized);
            return 1;
        }
    }
    lua::Stack::push(L, path);
    return 1;
}

int LocalizationLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"add", &lua::Binding::native<&add>}, {"loadFolder", &lua::Binding::native<&loadFolder>}, {"setLanguage", &lua::Binding::native<&setLanguage>}, {"language", &lua::Binding::native<&language>}, {"setFallback", &lua::Binding::native<&setFallback>}, {"fallback", &lua::Binding::native<&fallback>}, {"setPseudo", &lua::Binding::native<&setPseudo>}, {"pseudo", &lua::Binding::native<&pseudo>}, {"languages", &lua::Binding::native<&languages>}, {"has", &lua::Binding::native<&has>}, {"text", &lua::Binding::native<&text>}, {"findBestMatch", &lua::Binding::native<&findBestMatch>}, {"direction", &lua::Binding::native<&direction>}, {"formatNumber", &lua::Binding::native<&formatNumber>}, {"formatDate", &lua::Binding::native<&formatDate>}, {"formatList", &lua::Binding::native<&formatList>}, {"pluralCategory", &lua::Binding::native<&pluralCategory>}, {"assetPath", &lua::Binding::native<&assetPath>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void LocalizationLua::install(lua_State* L) {
    lua::Binding::preload(L, "haylen.localization", &open);
}

} // namespace haylen::localization
