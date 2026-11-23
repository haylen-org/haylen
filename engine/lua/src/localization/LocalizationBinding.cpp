#include "localization/LocalizationBinding.hpp"

#include <lua.hpp>

#include <cstdint>
#include <exception>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/content/ContentManager.hpp"
#include "haylen/content/Error.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/localization/DateFormat.hpp"
#include "haylen/localization/LanguageTag.hpp"
#include "haylen/localization/ListFormat.hpp"
#include "haylen/localization/NumberFormat.hpp"
#include "haylen/localization/PluralRules.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Promise.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/plugins/LocalizationPlugin.hpp"
#include "haylen/text/Style.hpp"

namespace haylen::lua::localization {

haylen::localization::Catalog& LocalizationBinding::getCatalog(lua_State* L) {
    return lua::Runtime::getEngine(L).getPlugin<haylen::plugins::LocalizationPlugin>().getCatalog();
}

int LocalizationBinding::add(lua_State* L) {
    getCatalog(L).add(lua::Stack::read<std::string>(L, 1), lua::JsonConverter::read(L, 2));
    return 0;
}

// A synchronous load never waits for a download, so a file that is not on the device yet fails before any file of the folder is added.
int LocalizationBinding::loadFolder(lua_State* L) {
    haylen::core::Engine& owner = lua::Runtime::getEngine(L);
    const auto folder = lua::Stack::read<std::string_view>(L, 1);
    for (const std::string& file : haylen::plugins::LocalizationPlugin::getLanguageFiles(owner.getPackage(), folder)) {
        if (!owner.getContent().isLocal(file)) {
            throw std::runtime_error("The localization file \"" + file + "\" is not on the device yet, since it downloads when the app asks for it. Load its folder with \"localization.loadFolderAsync\", which brings it first.");
        }
    }

    lua::Stack::push(L, owner.getPlugin<haylen::plugins::LocalizationPlugin>().loadFolder(owner.getPackage(), folder));
    return 1;
}

int LocalizationBinding::loadFolderAsync(lua_State* L) {
    haylen::core::Engine& owner = lua::Runtime::getEngine(L);
    std::string folder = lua::Stack::read<std::string>(L, 1);
    const std::vector<std::string> files = haylen::plugins::LocalizationPlugin::getLanguageFiles(owner.getPackage(), folder);
    const lua::Promise promise(owner);
    // clang-format off
    owner.getContent().acquire(files, {}, [&owner, promise, folder = std::move(folder)](std::optional<haylen::content::Error> failure) {
        if (failure) {
            promise.reject(failure->what());
            return;
        }

        try {
            const std::vector<std::string> languages = owner.getPlugin<haylen::plugins::LocalizationPlugin>().loadFolder(owner.getPackage(), folder);
            promise.resolve(haylen::core::Json(languages));
        } catch (const std::exception& error) {
            promise.reject(error.what());
        }
    });
    // clang-format on
    promise.push(L);
    return 1;
}

int LocalizationBinding::setLanguage(lua_State* L) {
    getCatalog(L).setLanguage(lua::Stack::read<std::string_view>(L, 1));
    return 0;
}

int LocalizationBinding::language(lua_State* L) {
    lua::Stack::push(L, getCatalog(L).getLanguage());
    return 1;
}

int LocalizationBinding::followSystem(lua_State* L) {
    getCatalog(L).followSystem();
    return 0;
}

int LocalizationBinding::followingSystem(lua_State* L) {
    lua::Stack::push(L, getCatalog(L).isFollowingSystem());
    return 1;
}

int LocalizationBinding::setFallback(lua_State* L) {
    getCatalog(L).setFallback(lua::Stack::read<std::string_view>(L, 1));
    return 0;
}

int LocalizationBinding::fallback(lua_State* L) {
    lua::Stack::push(L, getCatalog(L).getFallback());
    return 1;
}

int LocalizationBinding::setPseudo(lua_State* L) {
    getCatalog(L).setPseudo(lua::Stack::read<bool>(L, 1));
    return 0;
}

int LocalizationBinding::pseudo(lua_State* L) {
    lua::Stack::push(L, getCatalog(L).isPseudo());
    return 1;
}

int LocalizationBinding::languages(lua_State* L) {
    lua::Stack::push(L, getCatalog(L).getLanguages());
    return 1;
}

int LocalizationBinding::has(lua_State* L) {
    lua::Stack::push(L, getCatalog(L).has(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

// Translates with `text(key[, arguments])`, where `arguments` fill `{name}` placeholders and `count` picks the plural form.
int LocalizationBinding::text(lua_State* L) {
    const haylen::core::Json arguments = lua_isnoneornil(L, 2) ? haylen::core::Json::object() : lua::JsonConverter::read(L, 2);
    lua::Stack::push(L, getCatalog(L).getText(lua::Stack::read<std::string_view>(L, 1), arguments));
    return 1;
}

// Fills with `format(text[, arguments])` the placeholders of a text that is not a key, the way `text` fills the texts of the catalog.
int LocalizationBinding::format(lua_State* L) {
    const haylen::core::Json arguments = lua_isnoneornil(L, 2) ? haylen::core::Json::object() : lua::JsonConverter::read(L, 2);
    lua::Stack::push(L, haylen::localization::Catalog::format(lua::Stack::read<std::string_view>(L, 1), arguments));
    return 1;
}

// Returns with `direction([language])` the direction the current or the named language declares, `leftToRight` or `rightToLeft`.
int LocalizationBinding::direction(lua_State* L) {
    const haylen::localization::Catalog& catalog = getCatalog(L);
    const std::string language = lua_isnoneornil(L, 1) ? catalog.getLanguage() : lua::Stack::read<std::string>(L, 1);
    lua::Stack::push(L, haylen::text::Style::directionName(catalog.getDirection(language)));
    return 1;
}

int LocalizationBinding::findBestMatch(lua_State* L) {
    lua::Stack::push(L, getCatalog(L).findBestMatch(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int LocalizationBinding::normalizeTag(lua_State* L) {
    lua::Stack::push(L, haylen::localization::Catalog::normalizeTag(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

// The option takes a language tag in any form, so its canonical form picks the rules and the marks that the catalog would pick for it.
std::string LocalizationBinding::readLanguage(lua_State* L, int index) {
    if (!lua_istable(L, index)) {
        return getCatalog(L).getLanguage();
    }

    std::optional<std::string> language;
    lua::Table::readField(L, index, "language", language);
    return language ? haylen::localization::LanguageTag::normalize(*language) : getCatalog(L).getLanguage();
}

// Writes a number with `formatNumber(value[, {style, minimumFractionDigits, maximumFractionDigits, currency, grouping, language}])`.
int LocalizationBinding::formatNumber(lua_State* L) {
    const double value = luaL_checknumber(L, 1);
    haylen::localization::NumberFormat::Options options;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kNumberOptions});
        std::string style;
        lua::Table::readField(L, 2, "style", style);
        if (!style.empty()) {
            const std::optional<haylen::localization::NumberFormat::Style> found = haylen::localization::NumberFormat::styleFromName(style);
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

    lua::Stack::push(L, haylen::localization::NumberFormat::format(readLanguage(L, 2), value, options));
    return 1;
}

// Writes a moment in Unix seconds with `formatDate(seconds[, {date = 'medium', time = false, timeZoneOffset, language}])`, where `date = false` leaves the date out.
int LocalizationBinding::formatDate(lua_State* L) {
    const double moment = luaL_checknumber(L, 1);
    if (!(moment >= -0x1p63 && moment < 0x1p63)) {
        throw std::invalid_argument("A moment to format is a finite number of seconds within the range of 64-bit integers.");
    }

    const auto seconds = static_cast<std::int64_t>(moment);
    haylen::localization::DateFormat::Options options;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kDateOptions});
        if (lua_getfield(L, 2, "date") == LUA_TBOOLEAN && lua_toboolean(L, -1) == 0) {
            options.date.reset();
        } else if (!lua_isnil(L, -1)) {
            const std::optional<haylen::localization::DateFormat::Style> found = lua_type(L, -1) == LUA_TSTRING ? haylen::localization::DateFormat::styleFromName(lua_tostring(L, -1)) : std::nullopt;
            if (!found) {
                return luaL_error(L, "The date of a date format must be \"short\", \"medium\", \"long\", \"full\" or false.");
            }

            options.date = *found;
        }

        lua_pop(L, 1);
        lua::Table::readField(L, 2, "time", options.time);
        lua::Table::readField(L, 2, "timeZoneOffset", options.timeZoneOffset);
    }

    lua::Stack::push(L, haylen::localization::DateFormat::format(readLanguage(L, 2), seconds, options));
    return 1;
}

// Joins strings with `formatList(items[, {type = 'and', language}])`.
int LocalizationBinding::formatList(lua_State* L) {
    const std::vector<std::string> items = lua::Stack::read<std::vector<std::string>>(L, 1);
    haylen::localization::ListFormat::Type type = haylen::localization::ListFormat::Type::And;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kListOptions});
        std::string name;
        lua::Table::readField(L, 2, "type", name);
        if (!name.empty()) {
            const std::optional<haylen::localization::ListFormat::Type> found = haylen::localization::ListFormat::typeFromName(name);
            if (!found) {
                return luaL_error(L, "The type of a list must be \"and\" or \"or\".");
            }

            type = *found;
        }
    }

    lua::Stack::push(L, haylen::localization::ListFormat::format(readLanguage(L, 2), items, type));
    return 1;
}

// Returns the plural category of a count with `pluralCategory(count[, {ordinal = false, language}])`.
int LocalizationBinding::pluralCategory(lua_State* L) {
    const double count = luaL_checknumber(L, 1);
    bool ordinal = false;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kPluralOptions});
        lua::Table::readField(L, 2, "ordinal", ordinal);
    }

    const std::string language = readLanguage(L, 2);
    lua::Stack::push(L, haylen::localization::PluralRules::categoryName(ordinal ? haylen::localization::PluralRules::selectOrdinal(language, count) : haylen::localization::PluralRules::select(language, count)));
    return 1;
}

int LocalizationBinding::assetPath(lua_State* L) {
    haylen::core::Engine& engine = lua::Runtime::getEngine(L);
    lua::Stack::push(L, engine.getPlugin<haylen::plugins::LocalizationPlugin>().getAssetPath(engine.getAssets(), lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int LocalizationBinding::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"add", &lua::Binding::native<&add>}, {"loadFolder", &lua::Binding::native<&loadFolder>}, {"loadFolderAsync", &lua::Binding::native<&loadFolderAsync>}, {"setLanguage", &lua::Binding::native<&setLanguage>}, {"language", &lua::Binding::native<&language>}, {"followSystem", &lua::Binding::native<&followSystem>}, {"followingSystem", &lua::Binding::native<&followingSystem>}, {"setFallback", &lua::Binding::native<&setFallback>}, {"fallback", &lua::Binding::native<&fallback>}, {"setPseudo", &lua::Binding::native<&setPseudo>}, {"pseudo", &lua::Binding::native<&pseudo>}, {"languages", &lua::Binding::native<&languages>}, {"has", &lua::Binding::native<&has>}, {"text", &lua::Binding::native<&text>}, {"format", &lua::Binding::native<&format>}, {"findBestMatch", &lua::Binding::native<&findBestMatch>}, {"normalizeTag", &lua::Binding::native<&normalizeTag>}, {"direction", &lua::Binding::native<&direction>}, {"formatNumber", &lua::Binding::native<&formatNumber>}, {"formatDate", &lua::Binding::native<&formatDate>}, {"formatList", &lua::Binding::native<&formatList>}, {"pluralCategory", &lua::Binding::native<&pluralCategory>}, {"assetPath", &lua::Binding::native<&assetPath>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void LocalizationBinding::install(lua_State* L) {
    lua::Binding::preload(L, "haylen.localization", &open);
}

} // namespace haylen::lua::localization
