#pragma once

#include <array>
#include <string>
#include <string_view>

struct lua_State;

namespace haylen::localization {

class Catalog;

// Installs `haylen.localization`, which adds languages, picks the current one and translates keys.
class LocalizationLua final {
  public:
    static void install(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 6> kNumberOptions{"style", "minimumFractionDigits", "maximumFractionDigits", "currency", "grouping", "language"};
    static constexpr std::array<std::string_view, 4> kDateOptions{"date", "time", "timeZoneOffset", "language"};
    static constexpr std::array<std::string_view, 2> kListOptions{"type", "language"};
    static constexpr std::array<std::string_view, 2> kPluralOptions{"ordinal", "language"};

    [[nodiscard]] static Catalog& getCatalog(lua_State* L);

    // Reads the `language` option of the table at the index, the current language by default.
    [[nodiscard]] static std::string readLanguage(lua_State* L, int index);

    static int add(lua_State* L);
    static int loadFolder(lua_State* L);
    static int setLanguage(lua_State* L);
    static int language(lua_State* L);
    static int setFallback(lua_State* L);
    static int fallback(lua_State* L);
    static int setPseudo(lua_State* L);
    static int pseudo(lua_State* L);
    static int languages(lua_State* L);
    static int has(lua_State* L);
    static int text(lua_State* L);
    static int findBestMatch(lua_State* L);
    static int direction(lua_State* L);
    static int formatNumber(lua_State* L);
    static int formatDate(lua_State* L);
    static int formatList(lua_State* L);
    static int pluralCategory(lua_State* L);
    static int assetPath(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::localization
